cmake_minimum_required(VERSION 3.22.1)

project(forgeshape_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Shaders: GLSL -> SPIR-V ahead of time using the glslc that ships with the NDK.
# No shaderc runtime dependency, no downloaded compiler, no SPIR-V reflection.
# glslc's -mfmt=c emits a C initializer list which is #included directly by the
# renderer, so no asset loading is required at runtime.
# ---------------------------------------------------------------------------
find_program(FORGESHAPE_GLSLC
    NAMES glslc glslc.exe
    HINTS
        "${ANDROID_NDK}/shader-tools/${ANDROID_HOST_TAG}"
        "${ANDROID_NDK}/shader-tools/windows-x86_64"
        "${ANDROID_NDK}/shader-tools/linux-x86_64"
        "${ANDROID_NDK}/shader-tools/darwin-x86_64"
    NO_CMAKE_FIND_ROOT_PATH
    REQUIRED)
message(STATUS "ForgeShape: using glslc at ${FORGESHAPE_GLSLC}")

set(FORGESHAPE_SHADER_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders")
file(MAKE_DIRECTORY "${FORGESHAPE_SHADER_OUT_DIR}")

set(FORGESHAPE_SHADER_HEADERS "")
foreach(shader surface.vert surface.frag grid.vert grid.frag gizmo.vert gizmo.frag
                outline_mask.vert outline_mask.frag outline.vert outline.frag)
    set(_src "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}")
    set(_out "${FORGESHAPE_SHADER_OUT_DIR}/${shader}.spv.inc")
    add_custom_command(
        OUTPUT "${_out}"
        COMMAND "${FORGESHAPE_GLSLC}" -O --target-env=vulkan1.0 -mfmt=c "${_src}" -o "${_out}"
        DEPENDS "${_src}"
        COMMENT "ForgeShape: compiling ${shader} to SPIR-V"
        VERBATIM)
    list(APPEND FORGESHAPE_SHADER_HEADERS "${_out}")
endforeach()

add_custom_target(forgeshape_shaders DEPENDS ${FORGESHAPE_SHADER_HEADERS})

# ---------------------------------------------------------------------------
# Native library
# ---------------------------------------------------------------------------
# Product sources: everything the shipped `.so` needs in every configuration.
# The debug fixtures (`forgeshape_mesh_fixtures`, `forgeshape_glb_import_fixture`)
# stay here because the `#ifndef NDEBUG` debug entry points in forgeshape_jni.cpp
# reference them; they are not self-tests.
set(FORGESHAPE_PRODUCT_SOURCES
    forgeshape_jni.cpp
    forgeshape_camera.cpp
    forgeshape_cad_body.cpp
    forgeshape_cad_face.cpp
    forgeshape_cad_feature.cpp
    forgeshape_cad_revolve.cpp
    forgeshape_cad_timeline.cpp
    forgeshape_cad_kernel.cpp
    forgeshape_cad_extrude_tool.cpp
    forgeshape_sketch.cpp
    forgeshape_sketch_region.cpp
    forgeshape_sketch_arrangement.cpp
    forgeshape_sketch_overlay.cpp
    forgeshape_sketch_session.cpp
    forgeshape_sketch_dimension.cpp
    forgeshape_sketch_snap.cpp
    forgeshape_sketch_drafting.cpp
    forgeshape_support_chooser.cpp
    forgeshape_workplane.cpp
    forgeshape_body_dimension_overlay.cpp
    forgeshape_body_dimensions.cpp
    forgeshape_body_commands.cpp
    forgeshape_body_mirror.cpp
    forgeshape_body_delete.cpp
    forgeshape_construction.cpp
    forgeshape_demo_mesh.cpp
    forgeshape_display.cpp
    forgeshape_gizmo.cpp
    forgeshape_glb_import_fixture.cpp
    forgeshape_glb_roundtrip.cpp
    forgeshape_gltf_export.cpp
    forgeshape_gltf_import.cpp
    forgeshape_import_preview.cpp
    forgeshape_import_commit.cpp
    forgeshape_imported_mesh.cpp
    forgeshape_json.cpp
    forgeshape_grid.cpp
    forgeshape_history.cpp
    forgeshape_input.cpp
    forgeshape_matcap.cpp
    forgeshape_mesh.cpp
    forgeshape_mesh_fixtures.cpp
    forgeshape_picking.cpp
    forgeshape_project_bootstrap.cpp
    forgeshape_project_bytes.cpp
    forgeshape_project_document.cpp
    forgeshape_project_state.cpp
    forgeshape_render_mesh.cpp
    forgeshape_render_recovery.cpp
    forgeshape_scene.cpp
    forgeshape_sculpt.cpp
    forgeshape_sculpt_history.cpp
    forgeshape_selection.cpp
    forgeshape_selection_outline.cpp
    forgeshape_selection_pulse.cpp
    forgeshape_transform.cpp
    forgeshape_renderer.cpp)

# Self-test suites. Their only call sites are the `#ifndef NDEBUG` runners in
# forgeshape_jni.cpp, so a release `.so` must not carry their code, their check
# names or their exported entry points. Debug is the one configuration that
# leaves NDEBUG undefined, and it is the only one that compiles them.
set(FORGESHAPE_SELFTEST_SOURCES
    forgeshape_body_dimensions_selftest.cpp
    forgeshape_mirror_selftest.cpp
    forgeshape_camera_selftest.cpp
    forgeshape_cad_selftest.cpp
    forgeshape_cad_a3_selftest.cpp
    forgeshape_cad_feature_selftest.cpp
    forgeshape_cad_multiface_selftest.cpp
    forgeshape_cad_revolve_selftest.cpp
    forgeshape_cad_timeline_selftest.cpp
    forgeshape_sketch_drafting_selftest.cpp
    forgeshape_sketch_arrangement_selftest.cpp
    forgeshape_sketch_fill_selftest.cpp
    forgeshape_cad_v6_selftest.cpp
    forgeshape_sketch_ux_selftest.cpp
    forgeshape_construction_selftest.cpp
    forgeshape_cone_capsule_selftest.cpp
    forgeshape_gltf_export_selftest.cpp
    forgeshape_gltf_import_selftest.cpp
    forgeshape_gizmo_selftest.cpp
    forgeshape_history_selftest.cpp
    forgeshape_mesh_selftest.cpp
    forgeshape_project_selftest.cpp
    forgeshape_render_recovery_selftest.cpp
    forgeshape_render_mesh_selftest.cpp
    forgeshape_picking_selftest.cpp
    forgeshape_primitive_selftest.cpp
    forgeshape_scene_selftest.cpp
    forgeshape_sculpt_selftest.cpp
    forgeshape_sphere_selftest.cpp
    forgeshape_transform_selftest.cpp)

# ---------------------------------------------------------------------------
# The CAD boolean kernel (`CAD-VERTICAL-SLICE-R1`, GATE-KERNEL)
# ---------------------------------------------------------------------------
# Manifold 3.5.4 (Apache-2.0), vendored unmodified under third_party/manifold
# and reproducible with scripts/vendor-manifold.sh. A STATIC library linked into
# the one shared object, so there is no second .so whose availability could
# differ between Android and a future desktop build. Only
# forgeshape_cad_kernel.cpp includes its headers.
#   MANIFOLD_PAR=-1          single-threaded, no TBB: deterministic and small
#   MANIFOLD_NO_IOSTREAM     no OBJ reader/writer, no filesystem
#   (no MANIFOLD_DEBUG)      no exceptions thrown, no debug dumps
# Always optimised, in every configuration: it is third-party arithmetic that
# is never stepped through, and a debug build's self-tests run it on device.
file(GLOB FORGESHAPE_MANIFOLD_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/manifold/src/*.cpp")
add_library(forgeshape_manifold STATIC ${FORGESHAPE_MANIFOLD_SOURCES})
target_include_directories(forgeshape_manifold PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/manifold/include")
target_compile_definitions(forgeshape_manifold PUBLIC
    MANIFOLD_PAR=-1 MANIFOLD_NO_IOSTREAM MANIFOLD_NO_FILESYSTEM)
target_compile_options(forgeshape_manifold PRIVATE -O2 -w)
set_target_properties(forgeshape_manifold PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(forgeshape_native SHARED ${FORGESHAPE_PRODUCT_SOURCES})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_sources(forgeshape_native PRIVATE ${FORGESHAPE_SELFTEST_SOURCES})
    message(STATUS "ForgeShape: debug build, self-test suites compiled in")
else()
    message(STATUS "ForgeShape: ${CMAKE_BUILD_TYPE} build, self-test suites excluded")
endif()

add_dependencies(forgeshape_native forgeshape_shaders)

target_include_directories(forgeshape_native PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${FORGESHAPE_SHADER_OUT_DIR}")

find_library(ANDROID_LIB android)
find_library(LOG_LIB log)
find_library(VULKAN_LIB vulkan)

target_link_libraries(forgeshape_native
    forgeshape_manifold
    ${VULKAN_LIB}
    ${ANDROID_LIB}
    ${LOG_LIB})
