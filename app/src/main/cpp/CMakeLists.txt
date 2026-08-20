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
foreach(shader surface.vert surface.frag)
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
add_library(forgeshape_native SHARED
    forgeshape_jni.cpp
    forgeshape_camera.cpp
    forgeshape_camera_selftest.cpp
    forgeshape_construction.cpp
    forgeshape_construction_selftest.cpp
    forgeshape_cone_capsule_selftest.cpp
    forgeshape_demo_mesh.cpp
    forgeshape_display.cpp
    forgeshape_matcap.cpp
    forgeshape_mesh.cpp
    forgeshape_mesh_fixtures.cpp
    forgeshape_mesh_selftest.cpp
    forgeshape_picking.cpp
    forgeshape_render_mesh.cpp
    forgeshape_render_mesh_selftest.cpp
    forgeshape_picking_selftest.cpp
    forgeshape_primitive_selftest.cpp
    forgeshape_sculpt.cpp
    forgeshape_sculpt_selftest.cpp
    forgeshape_selection.cpp
    forgeshape_sphere_selftest.cpp
    forgeshape_transform.cpp
    forgeshape_transform_selftest.cpp
    forgeshape_renderer.cpp)

add_dependencies(forgeshape_native forgeshape_shaders)

target_include_directories(forgeshape_native PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${FORGESHAPE_SHADER_OUT_DIR}")

find_library(ANDROID_LIB android)
find_library(LOG_LIB log)
find_library(VULKAN_LIB vulkan)

target_link_libraries(forgeshape_native
    ${VULKAN_LIB}
    ${ANDROID_LIB}
    ${LOG_LIB})
