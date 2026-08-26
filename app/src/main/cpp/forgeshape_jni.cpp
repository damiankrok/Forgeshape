// Minimal JNI boundary for the ForgeShape native viewport.
//
// Responsibilities, and nothing more:
//   - own the render thread;
//   - convert the Java Surface to an ANativeWindow;
//   - forward surface create/resize/destroy to the renderer;
//   - guarantee the renderer never touches a destroyed ANativeWindow;
//   - translate Android MotionEvent data into platform-neutral touch events for
//     the CameraController and the SelectionController, and hand the resulting
//     camera snapshot and selection visual state to the renderer once per frame.
//
// It owns no camera math, no gesture semantics, no picking math and no selected
// identity; those live in forgeshape_camera.{h,cpp}, forgeshape_picking.{h,cpp}
// and forgeshape_selection.{h,cpp}.

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <atomic>
#include <cstdio>

#include "forgeshape_camera.h"
#include "forgeshape_camera_selftest.h"
#include "forgeshape_construction.h"
#include "forgeshape_construction_selftest.h"
#include "forgeshape_display.h"
#include "forgeshape_history.h"
#include "forgeshape_history_selftest.h"
#include "forgeshape_input.h"
#include "forgeshape_mesh.h"
#include "forgeshape_mesh_fixtures.h"
#include "forgeshape_mesh_selftest.h"
#include "forgeshape_picking_selftest.h"
#include "forgeshape_primitive_selftest.h"
#include "forgeshape_render_mesh_selftest.h"
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
};

ViewportThread g_viewport;

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
// A one-finger Down on the Frozen Sculpt Mesh is ambiguous at the moment it
// arrives: it is either the start of a brush stroke, or the first of the two
// fingers of a pan/pinch. Android cannot tell us which, because the second
// finger has not landed yet.
//
// Stage 012 resolved it optimistically — the stroke began on Down — so a
// two-finger gesture whose first finger happened to land on the mesh committed a
// stroke that a moment later ended having moved nothing. It was harmless in
// practice but it was a real stroke: it advanced the stroke counter and it was
// one brush event away from deforming geometry.
//
// The rule now is PENDING-then-promote, decided entirely in native code:
//
//   Down, one finger, hits the mesh   -> PENDING. The event is swallowed:
//                                        nothing is deformed, no stroke exists,
//                                        and neither the camera nor the
//                                        selection sees it.
//   Move, still one finger, travelled
//   at least kStrokeArmPixels         -> PROMOTE. The stroke begins at the
//                                        ORIGINAL down point, so the anchor, the
//                                        hit and the affected set are exactly
//                                        what Stage 012 would have captured.
//   Anything else (a second finger,
//   an Up, a Cancel, a multi-pointer
//   event)                            -> ABANDON. No stroke ever existed, so
//                                        there is nothing to end and nothing to
//                                        undo, and the gesture becomes ordinary
//                                        navigation from that event onward.
//
// Because the camera re-anchors on any pointer-set change, handing it a gesture
// mid-flight produces no jump; because the selection never saw a Down, an
// abandoned pending gesture cannot resolve a tap either.
//
// Both flags are guarded by g_stateMutex, like the camera and the selection,
// because the same touch event decides between brushing and navigating and the
// two must not disagree. The product mode, the active tool and the Frozen Sculpt
// Mesh themselves live in the process-scoped SculptSession; these are only
// gesture routing, and they are dropped whenever the Surface goes away, exactly
// as camera anchors and tap candidacy are.

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
    constexpr int kMaxSculptChecks = 512;
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
    forgeshape::TransformSelfTestResult results[128];
    const int count = forgeshape::runTransformSelfTests(results, 128);
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
    forgeshape::ConstructionObject& object = forgeshape::constructionObject();
    const forgeshape::ConstructionMesh mesh = object.generateMesh();
    forgeshape::MeshValidation why = forgeshape::MeshValidation::Ok;
    const forgeshape::MeshRevision revision =
        forgeshape::publishConstructionObject(object, forgeshape::meshStore(), &why);
    if (revision == forgeshape::kNoMeshRevision) {
        FS_LOGE("FORGESHAPE_CONSTRUCTION_PUBLISH_FAIL:%s:%s", reason,
                forgeshape::meshValidationName(why));
        return revision;
    }
    char described[128];
    describeSpec(object.spec(), described, sizeof(described));
    FS_LOGI("FORGESHAPE_CONSTRUCTION_PUBLISHED:%llu:%u:%u kind=%s %s objectId=%llu reason=%s",
            (unsigned long long)revision, static_cast<uint32_t>(mesh.vertices.size()),
            static_cast<uint32_t>(mesh.indices.size()),
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
forgeshape::MeshRevision publishSculptRepresentation(const char* reason) {
    forgeshape::SculptSession& session = forgeshape::sculptSession();
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
    if (forgeshape::sculptSession().inSculptMode()) {
        return publishSculptRepresentation(reason);
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
    const forgeshape::ConstructionObject& object = forgeshape::constructionObject();

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
                publishSculptRepresentation("construction_changed_in_sculpt_mode");
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

// Applies an authoritative transform through the one Construction transform
// entry point and reports what happened.
//
// It publishes nothing and uploads nothing, by construction: the entry point has
// no access to the mesh store. The box's RuntimeMesh is its LOCAL geometry, so
// moving or rotating it changes only the derived matrix the renderer and the
// picker read.
forgeshape::TransformApplyResult applyBoxTransform(const char* label,
                                                   const forgeshape::TransformValues& requested) {
    forgeshape::TransformApplyResult result;
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
        forgeshape::ScopedConstructionEdit edit(forgeshape::constructionHistory());
        result = forgeshape::applyConstructionTransform(requested);
    }
    const forgeshape::TransformValues& v = result.values;
    const forgeshape::ConstructionTransform& transform = forgeshape::constructionTransform();
    switch (result.status) {
        case forgeshape::TransformUpdateStatus::Applied:
            FS_LOGI("FORGESHAPE_CONSTRUCTION_TRANSFORM:%s pos=(%.6f,%.6f,%.6f)m "
                    "rot=(%.6f,%.6f,%.6f)deg updates=%llu rev=%llu",
                    label, v.positionX, v.positionY, v.positionZ, v.rotationX, v.rotationY,
                    v.rotationZ, (unsigned long long)transform.updateCount(),
                    (unsigned long long)forgeshape::meshStore().currentRevision());
            break;
        case forgeshape::TransformUpdateStatus::Unchanged:
            FS_LOGI("FORGESHAPE_CONSTRUCTION_TRANSFORM_UNCHANGED:%s pos=(%.6f,%.6f,%.6f)m "
                    "rot=(%.6f,%.6f,%.6f)deg rev=%llu",
                    label, v.positionX, v.positionY, v.positionZ, v.rotationX, v.rotationY,
                    v.rotationZ, (unsigned long long)forgeshape::meshStore().currentRevision());
            break;
        case forgeshape::TransformUpdateStatus::Rejected:
            FS_LOGE("FORGESHAPE_CONSTRUCTION_TRANSFORM_REJECTED:%s:%s "
                    "requested=(%.6f,%.6f,%.6f,%.6f,%.6f,%.6f) retained pos=(%.6f,%.6f,%.6f)m "
                    "rot=(%.6f,%.6f,%.6f)deg rev=%llu rejects=%llu",
                    label, forgeshape::transformValidationName(result.validation),
                    requested.positionX, requested.positionY, requested.positionZ,
                    requested.rotationX, requested.rotationY, requested.rotationZ, v.positionX,
                    v.positionY, v.positionZ, v.rotationX, v.rotationY, v.rotationZ,
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
void logSculptState(const char* reason) {
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

void logMeshDiagnostics(const char* reason) {
    const forgeshape::MeshGpuStats s = forgeshape::meshUploadDiagnostics().snapshot();
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
    // inferring dimensions from vertices.
    const forgeshape::ConstructionObject& object = forgeshape::constructionObject();
    char described[128];
    describeSpec(object.spec(), described, sizeof(described));
    FS_LOGI("FORGESHAPE_CONSTRUCTION_STATE:%s kind=%s %s objectId=%llu updates=%llu rejects=%llu",
            reason, forgeshape::primitiveKindName(object.kind()), described,
            (unsigned long long)object.objectId(), (unsigned long long)object.updateCount(),
            (unsigned long long)object.rejectedUpdateCount());
    // Placement is reported next to the dimensions and the mesh numbers, so one
    // line pair shows both truths and it is obvious that the transform's update
    // count is independent of the mesh revision.
    forgeshape::TransformValues t;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        t = forgeshape::constructionTransform().values();
    }
    const forgeshape::ConstructionTransform& transform = forgeshape::constructionTransform();
    FS_LOGI("FORGESHAPE_CONSTRUCTION_TRANSFORM_STATE:%s pos=(%.6f,%.6f,%.6f)m "
            "rot=(%.6f,%.6f,%.6f)deg identity=%d updates=%llu rejects=%llu",
            reason, t.positionX, t.positionY, t.positionZ, t.rotationX, t.rotationY, t.rotationZ,
            transform.isIdentity() ? 1 : 0, (unsigned long long)transform.updateCount(),
            (unsigned long long)transform.rejectedUpdateCount());
    logSculptState(reason);
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
                renderer.setScene(forgeshape::constructionScene().snapshot());
            }
            // Presentation only, and deliberately OUTSIDE the state mutex: the
            // display settings are plain atomics that no domain invariant
            // depends on, so a frame must never wait on the geometry lock to
            // find out which shading model to draw with.
            renderer.setDisplaySettings(forgeshape::displaySettings().snapshot());
            if (!renderer.drawFrame()) {
                FS_LOGE("Render loop stopping after frame failure");
                renderer.detachSurface();
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
    // Product geometry must exist before anything picks against it: picking
    // reads the current CPU mesh snapshot, and an empty store is a miss. Since
    // Stage 007 that geometry is generated by the Construction box, not by a
    // debug fixture.
    publishConstructionObject("startup");
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
    // The mesh and construction self-tests publish revisions of their own into
    // the store, so republish the ACTIVE representation: the app must always
    // come up showing what the current product mode says it is showing. At a
    // cold start that is always the Construction object, because nothing has
    // been frozen yet.
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
    const auto start = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_grabbing = false;
        g_strokePending = false;
        froze = forgeshape::sculptSession().freezeToSculpt(
            source, forgeshape::kConstructionBoxObjectId, &why);
    }
    const double freezeMs = std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - start)
                                .count();
    if (!froze) {
        FS_LOGE("FORGESHAPE_STRESS_SCULPT_FREEZE_FAIL:%s:%s", g_lastStressLabel,
                forgeshape::meshValidationName(why));
        return;
    }
    const forgeshape::MeshRevision revision = publishSculptRepresentation("stress_freeze");
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
    const forgeshape::ConstructionObject& object = forgeshape::constructionObject();
    const forgeshape::BoxDimensionsMeters box = object.box().dimensionsMeters();
    const forgeshape::CylinderDimensionsMeters cylinder = object.cylinder().dimensionsMeters();
    const forgeshape::SphereDimensionsMeters sphere = object.sphere().dimensionsMeters();
    const forgeshape::ConeDimensionsMeters cone = object.cone().dimensionsMeters();
    const forgeshape::CapsuleDimensionsMeters capsule = object.capsule().dimensionsMeters();
    const forgeshape::PlaneDimensionsMeters plane = object.plane().dimensionsMeters();
    const jdouble values[kSlots] = {
        static_cast<jdouble>(static_cast<int>(object.kind())),
        box.width, box.height, box.depth,
        cylinder.diameter, cylinder.height,
        sphere.diameter,
        cone.bottomDiameter, cone.height,
        capsule.diameter, capsule.totalHeight,
        plane.width, plane.depth,
    };
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
// meters, rotation in degrees. Nothing is recovered from the model matrix.
JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_boxTransform(JNIEnv* env, jclass,
                                                    jdoubleArray outPositionRotation) {
    if (outPositionRotation == nullptr || env->GetArrayLength(outPositionRotation) < 6) {
        return;
    }
    forgeshape::TransformValues v;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        v = forgeshape::constructionTransform().values();
    }
    const jdouble values[6] = {v.positionX, v.positionY, v.positionZ,
                               v.rotationX, v.rotationY, v.rotationZ};
    env->SetDoubleArrayRegion(outPositionRotation, 0, 6, values);
}

// The product transform edit path. One call carries all six values — position in
// meters, rotation in degrees — into the one native Construction transform entry
// point. It publishes no mesh revision and triggers no upload.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_applyBoxTransform(JNIEnv*, jclass, jdouble positionX,
                                                          jdouble positionY, jdouble positionZ,
                                                          jdouble rotationX, jdouble rotationY,
                                                          jdouble rotationZ) {
    forgeshape::TransformValues requested;
    requested.positionX = positionX;
    requested.positionY = positionY;
    requested.positionZ = positionZ;
    requested.rotationX = rotationX;
    requested.rotationY = rotationY;
    requested.rotationZ = rotationZ;
    return transformResultToJni(applyBoxTransform("ui", requested));
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

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_productMode(JNIEnv*, jclass) {
    return forgeshape::sculptSession().inSculptMode() ? 1 : 0;
}

// Freeze to Sculpt.
//
// One explicit user act performs the whole thing: take a coherent snapshot of
// the Construction object's CURRENT local mesh, make it the Frozen Sculpt Mesh
// with the same ObjectId, enter Sculpt mode, and publish it as the active
// representation. The Construction Source is only read, never written.
JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_freezeToSculpt(JNIEnv*, jclass) {
    const forgeshape::ConstructionObject& object = forgeshape::constructionObject();
    const forgeshape::ConstructionMesh source = object.generateMesh();
    forgeshape::MeshValidation why = forgeshape::MeshValidation::Ok;

    bool froze = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_grabbing = false;
        g_strokePending = false;
        froze = forgeshape::sculptSession().freezeToSculpt(source, object.objectId(), &why);
    }
    if (!froze) {
        FS_LOGE("FORGESHAPE_SCULPT_FREEZE_FAIL:%s", forgeshape::meshValidationName(why));
        return kSculptFailedFreeze;
    }

    const forgeshape::SculptMesh& mesh = forgeshape::sculptSession().mesh();
    char described[128];
    describeSpec(object.spec(), described, sizeof(described));
    FS_LOGI("FORGESHAPE_SCULPT_FROZEN:%u:%u sculptRev=%llu objectId=%llu from kind=%s %s",
            mesh.vertexCount(), mesh.indexCount(), (unsigned long long)mesh.revision(),
            (unsigned long long)mesh.objectId(), forgeshape::primitiveKindName(object.kind()),
            described);
    const forgeshape::MeshRevision revision = publishSculptRepresentation("freeze");
    FS_LOGI("FORGESHAPE_SCULPT_MODE:sculpt meshRev=%llu", (unsigned long long)revision);
    logSculptState("freeze");
    return kSculptOk;
}

// Returns to Construction mode and republishes the Construction Source's own
// generated mesh, so what is on screen is the original object, unsculpted. The
// Frozen Sculpt Mesh is kept, untouched.
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
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        entered = forgeshape::sculptSession().enterSculpt();
        g_grabbing = false;
        g_strokePending = false;
    }
    if (!entered) {
        FS_LOGI("FORGESHAPE_SCULPT_MODE_REFUSED:nothing_frozen");
        return kSculptNothingFrozen;
    }
    const forgeshape::MeshRevision revision = publishSculptRepresentation("mode_sculpt");
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
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (forgeshape::sculptSession().inSculptMode()) {
            refusedInSculpt = true;
        } else {
            selected = forgeshape::constructionScene().setActiveBody(
                static_cast<forgeshape::ObjectId>(objectId));
        }
    }
    if (refusedInSculpt) {
        FS_LOGI("FORGESHAPE_SCENE_SELECT_REFUSED:in_sculpt_mode");
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
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (forgeshape::sculptSession().inSculptMode()) {
            refusedInSculpt = true;
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
    // Outside the lock: publication generates a mesh, and no lock is held
    // across geometry generation anywhere else either.
    const forgeshape::MeshRevision revision = publishConstructionObject("body_added");
    FS_LOGI("FORGESHAPE_SCENE_BODY_ADDED:%llu meshRev=%llu bodies=%d",
            (unsigned long long)created, (unsigned long long)revision,
            (int)forgeshape::constructionScene().bodyCount());
    return static_cast<jlong>(created);
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
// Undo and Redo are refused while sculpting, and the workspace withdraws the
// controls there — see the Sculpt separation in ARCHITECTURE.md. The refusal
// stays regardless: removing a control is not removing a guard.

// History status codes handed back to the Android UI. A JNI transport detail,
// in step with NativeViewport's HISTORY_* fields.
constexpr jint kHistoryOk = 0;
constexpr jint kHistoryNothingToDo = 1;
constexpr jint kHistoryRefusedInSculpt = 2;

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
    return static_cast<jlong>(forgeshape::meshStore().currentRevision());
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
    if (forgeshape::sculptSession().inSculptMode()) {
        FS_LOGI("FORGESHAPE_CONSTRUCTION_HISTORY_REFUSED:%s:in_sculpt_mode", label);
        return kHistoryRefusedInSculpt;
    }
    forgeshape::ConstructionRestoreReport report;
    bool moved = false;
    size_t undoDepth = 0;
    size_t redoDepth = 0;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        forgeshape::ConstructionHistory& history = forgeshape::constructionHistory();
        moved = forward ? history.redo(&report) : history.undo(&report);
        undoDepth = history.undoDepth();
        redoDepth = history.redoDepth();
    }
    if (!moved) {
        FS_LOGI("FORGESHAPE_CONSTRUCTION_HISTORY_EMPTY:%s", label);
        return kHistoryNothingToDo;
    }
    FS_LOGI("FORGESHAPE_CONSTRUCTION_HISTORY:%s republished=%d placements=%d restored=%d "
            "removed=%d activeChanged=%d undo=%d redo=%d bodies=%d",
            label, report.republishedBodies, report.replacedPlacements, report.restoredBodies,
            report.removedBodies, report.activeBodyChanged ? 1 : 0, (int)undoDepth,
            (int)redoDepth, (int)forgeshape::constructionScene().bodyCount());
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
//  [10] active tool (0 grab, 1 clay, 2 smooth, 3 inflate)
//  [11] 1 when the CURRENT Frozen Sculpt Mesh has user edits. This is the one
//       the re-Freeze guard asks, so the Java layer decides nothing about what
//       counts as an edit and holds no copy of the rule — see
//       SculptMesh::hasEdits().
JNIEXPORT void JNICALL
Java_com_forgeshape_app_NativeViewport_sculptState(JNIEnv* env, jclass, jdoubleArray outState) {
    constexpr jsize kSculptStateSize = 12;
    if (outState == nullptr || env->GetArrayLength(outState) < kSculptStateSize) {
        return;
    }
    const forgeshape::SculptSession& session = forgeshape::sculptSession();
    const forgeshape::SculptMesh& mesh = session.mesh();
    const jdouble values[kSculptStateSize] = {
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
    };
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
    forgeshape::SculptSession& session = forgeshape::sculptSession();
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        session.setRadiusPixels(static_cast<float>(radiusPixels));
        session.setStrength(static_cast<float>(strength));
    }
    FS_LOGI("FORGESHAPE_SCULPT_BRUSH:%s radiusPx=%.1f strength=%.3f",
            forgeshape::sculptToolName(session.tool()), session.radiusPixels(),
            session.strength());
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
    forgeshape::SculptSession& session = forgeshape::sculptSession();
    forgeshape::SculptTool requested = session.tool();
    const bool known = forgeshape::sculptToolFromIndex(static_cast<int>(toolIndex), &requested);
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (known) {
            session.setTool(requested);
        }
    }
    FS_LOGI("FORGESHAPE_SCULPT_TOOL:%s requested=%d known=%d",
            forgeshape::sculptToolName(session.tool()), static_cast<int>(toolIndex),
            known ? 1 : 0);
    return static_cast<jint>(forgeshape::sculptToolIndex(session.tool()));
}

JNIEXPORT jint JNICALL
Java_com_forgeshape_app_NativeViewport_sculptTool(JNIEnv*, jclass) {
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
        env->GetIntArrayRegion(ids, 0, count, idBuf);
        env->GetFloatArrayRegion(xs, 0, count, xBuf);
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

    // Brush-stroke reporting, gathered under the lock and logged outside it.
    bool grabBegan = false;
    bool grabEnded = false;
    bool grabCancelled = false;
    bool grabPublished = false;
    bool grabPending = false;
    bool grabAbandoned = false;
    const char* grabTool = "";
    int grabVertices = 0;
    float grabLocalRadius = 0.0f;
    forgeshape::SculptRevision grabRevision = 0;
    forgeshape::MeshRevision grabMeshRevision = 0;
    forgeshape::Vec3 grabDisplacement{};

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
                            grabLocalRadius = sculpt.stroke().localRadius();
                            // This same event is the stroke's first move, so no
                            // pointer travel is lost to the deferral.
                            if (sculpt.updateStroke(pointers[0].x, pointers[0].y)) {
                                grabRevision = sculpt.mesh().revision();
                                grabDisplacement = sculpt.stroke().lastLocalDisplacement();
                                grabMeshRevision = publishSculptRepresentation(
                                    forgeshape::sculptToolName(sculpt.stroke().tool()));
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
                                grabMeshRevision = publishSculptRepresentation(grabTool);
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

        if (!grabHandled) {
            g_camera.onTouch(translated, static_cast<int32_t>(actionPointerId),
                             count > 0 ? pointers : nullptr, count);

            // The selection owner sees exactly the same event and decides on its
            // own whether this gesture is still a tap. Navigation never consults
            // it and it never moves the camera.
            float tapX = 0.0f;
            float tapY = 0.0f;
            tapResolved = g_selection.onTouch(translated, static_cast<int32_t>(actionPointerId),
                                              count > 0 ? pointers : nullptr, count, &tapX, &tapY);
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
    if (grabPending) {
        FS_LOGI("FORGESHAPE_SCULPT_STROKE_PENDING sculptRev=%llu",
                (unsigned long long)forgeshape::sculptSession().mesh().revision());
    }
    if (grabAbandoned) {
        FS_LOGI("FORGESHAPE_SCULPT_STROKE_ABANDONED:navigation sculptRev=%llu",
                (unsigned long long)forgeshape::sculptSession().mesh().revision());
    }
    if (grabBegan) {
        FS_LOGI("FORGESHAPE_SCULPT_STROKE_BEGIN:%s:%d radiusLocal=%.4f", grabTool, grabVertices,
                grabLocalRadius);
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
