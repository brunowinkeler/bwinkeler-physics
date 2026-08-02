cmake_minimum_required(VERSION 3.24)

set(CMAKE_POLICY_VERSION_MINIMUM 3.10)

project(PhysicsLabWeb
    VERSION 0.1.0
    LANGUAGES C CXX
)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "PhysicsLabWeb must be configured with the Emscripten toolchain.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PHYSICS_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vendor/cpp-physics-simulation")
if(NOT EXISTS "${PHYSICS_SOURCE_ROOT}/project2_pendulum/src/simulation/PendulumSession.h")
    message(FATAL_ERROR "The physics source submodule is missing. Run git submodule update --init --recursive.")
endif()

include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(CUSTOMIZE_BUILD OFF CACHE BOOL "" FORCE)
set(PLATFORM Web CACHE STRING "" FORCE)
set(GRAPHICS GRAPHICS_API_OPENGL_ES2 CACHE STRING "" FORCE)
set(RAYLIB_BUILD_MODE Release CACHE STRING "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(physics_lab_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(physics_lab_imgui PUBLIC ${imgui_SOURCE_DIR})

FetchContent_Declare(
    rlimgui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG 286e11acd6c785004c9550c7ed3762add2ae3d47
)
FetchContent_MakeAvailable(rlimgui)

set(PENDULUM_NATIVE_SOURCES
    project2_pendulum/src/physics/SimplePendulum.cpp
    project2_pendulum/src/physics/DoublePendulum.cpp
    project2_pendulum/src/rendering/PendulumBackgroundRenderer.cpp
    project2_pendulum/src/rendering/PendulumCamera.cpp
    project2_pendulum/src/rendering/PendulumOverlayRenderer.cpp
    project2_pendulum/src/rendering/PendulumRenderer.cpp
    project2_pendulum/src/rendering/PendulumSceneRenderer.cpp
    project2_pendulum/src/rendering/PendulumTrajectoryRenderer.cpp
    project2_pendulum/src/simulation/PendulumSimulation.cpp
    project2_pendulum/src/ui/PendulumUiMenus.cpp
    core/physics/IntegrationMethod.cpp
    core/simulation/TrajectoryRecorder.cpp
)

set(PENDULUM_WEB_SOURCES web/pendulum/main.cpp)
foreach(source IN LISTS PENDULUM_NATIVE_SOURCES)
    list(APPEND PENDULUM_WEB_SOURCES "${PHYSICS_SOURCE_ROOT}/${source}")
endforeach()

add_executable(physics_lab_pendulum
    ${PENDULUM_WEB_SOURCES}
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
)

set_source_files_properties(
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
    PROPERTIES COMPILE_OPTIONS "-w"
)

target_include_directories(physics_lab_pendulum PRIVATE
    ${PHYSICS_SOURCE_ROOT}
    ${PHYSICS_SOURCE_ROOT}/project2_pendulum/src
    ${rlimgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
)

target_compile_definitions(physics_lab_pendulum PRIVATE
    NO_FONT_AWESOME
    PLATFORM_WEB
)

target_compile_options(physics_lab_pendulum PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-flto>
)

target_link_libraries(physics_lab_pendulum PRIVATE
    raylib
    physics_lab_imgui
)

target_link_options(physics_lab_pendulum PRIVATE
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-flto>
    "-sWASM=1"
    "-sENVIRONMENT=web"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_HEAP=67108864"
    "-sMAXIMUM_MEMORY=268435456"
    "-sDYNAMIC_EXECUTION=0"
    "-sASSERTIONS=$<IF:$<CONFIG:Release>,0,1>"
)

set(PENDULUM_RUNTIME_DIRECTORY
    "${CMAKE_CURRENT_SOURCE_DIR}/public/runtime/pendulum"
)
set_target_properties(physics_lab_pendulum PROPERTIES
    OUTPUT_NAME pendulum
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${PENDULUM_RUNTIME_DIRECTORY}"
)
