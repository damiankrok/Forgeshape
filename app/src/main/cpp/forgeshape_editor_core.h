// The editor state both shells share (`DESKTOP-R1`).
//
// What the Android JNI layer used to own as file-scoped globals and the
// Windows shell needs too: the camera, the selection, the one mutex that makes
// a tap's pick and the render thread's frame see one consistent state, and the
// handful of read-only rules a frame and a shell both ask. Platform-neutral:
// no JNI, no Win32, no Vulkan type crosses this header, so the host self-test
// build compiles it like any other domain file.
//
// Glue moves here act by act (`docs/DESKTOP_AND_TABLET_PLAN.md`); what still
// lives in a shell reaches a frame through `ViewportFrameSources`.
#pragma once

#include <mutex>

#include "forgeshape_body_dimensions.h"
#include "forgeshape_camera.h"
#include "forgeshape_scene.h"
#include "forgeshape_selection.h"

namespace forgeshape {

// Process-scoped, created on first use, never destroyed before exit: the
// camera pose and the selected body survive every surface, exactly as the
// JNI's globals did.
CameraController& editorCamera();
SelectionController& editorSelection();

// Guards the camera, the selection and every read of the scene a frame or a
// shell makes. One mutex for both, because a tap resolves a pick against the
// camera snapshot and then updates the selection.
std::mutex& editorStateMutex();

// The ACTIVE body's local bounds and overall dimensions, read off its source.
// Called under editorStateMutex().
bool editorActiveBodyDimensions(LocalBounds* outBounds, BodyDimensions* outDimensions);

// Whether the ACTIVE body is one Dimensions mode may stand on right now: the
// ONE predicate a frame closes the mode with and every shell read asks first.
// Called under editorStateMutex().
bool editorActiveBodyDimensionsEditable(LocalBounds* outBounds, BodyDimensions* outDimensions);

// Substitutes the sketch session's candidate (`CAD-VERTICAL-SLICE-R1`, "the
// preview IS the candidate") into the RENDERER's copy of the scene list, or
// appends it for a New Body. Never the list picking, saving or export read.
// Called under editorStateMutex(), on the thread that draws.
void applyCadOperationPreview(SceneSnapshot* list);

}  // namespace forgeshape
