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

set(BALLISTICS_NATIVE_SOURCES
    project1_balistica/src/physics/Projectile.cpp
    project1_balistica/src/rendering/BackgroundRenderer.cpp
    project1_balistica/src/rendering/Camera.cpp
    project1_balistica/src/rendering/ProjectileCoordinatesAdapter.cpp
    project1_balistica/src/rendering/ProjectileRenderer.cpp
    project1_balistica/src/rendering/SimulationOverlayRenderer.cpp
    project1_balistica/src/rendering/SimulationSceneRenderer.cpp
    project1_balistica/src/rendering/TrajectoryRenderer.cpp
    project1_balistica/src/simulation/Simulation.cpp
    project1_balistica/src/ui/UiMenus.cpp
    core/physics/IntegrationMethod.cpp
    core/simulation/TrajectoryRecorder.cpp
)

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

set(GAS_BROWNIAN_NATIVE_SOURCES
    project3_gas_brownian/src/physics/GasCollision.cpp
    project3_gas_brownian/src/physics/GasPreset.cpp
    project3_gas_brownian/src/rendering/GasCamera.cpp
    project3_gas_brownian/src/rendering/GasOverlayRenderer.cpp
    project3_gas_brownian/src/rendering/GasSceneRenderer.cpp
    project3_gas_brownian/src/simulation/GasSimulation.cpp
    project3_gas_brownian/src/ui/GasUiMenus.cpp
    core/simulation/TrajectoryRecorder.cpp
)

set_source_files_properties(
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
    PROPERTIES COMPILE_OPTIONS "-w"
)

# Every simulation ships an independently loaded bundle so a visitor downloads
# only the laboratory that was opened.
function(physics_lab_add_simulation)
    cmake_parse_arguments(SIMULATION
        ""
        "TARGET;RUNTIME_NAME;ENTRY_POINT;PROJECT_DIRECTORY"
        "NATIVE_SOURCES"
        ${ARGN}
    )

    set(simulation_sources "${CMAKE_CURRENT_SOURCE_DIR}/${SIMULATION_ENTRY_POINT}")
    foreach(source IN LISTS SIMULATION_NATIVE_SOURCES)
        list(APPEND simulation_sources "${PHYSICS_SOURCE_ROOT}/${source}")
    endforeach()

    add_executable(${SIMULATION_TARGET}
        ${simulation_sources}
        ${rlimgui_SOURCE_DIR}/rlImGui.cpp
    )

    target_include_directories(${SIMULATION_TARGET} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/web
        ${PHYSICS_SOURCE_ROOT}
        ${PHYSICS_SOURCE_ROOT}/${SIMULATION_PROJECT_DIRECTORY}/src
        ${rlimgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}
    )

    target_compile_definitions(${SIMULATION_TARGET} PRIVATE
        NO_FONT_AWESOME
        PLATFORM_WEB
    )

    target_compile_options(${SIMULATION_TARGET} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-flto>
    )

    target_link_libraries(${SIMULATION_TARGET} PRIVATE
        raylib
        physics_lab_imgui
    )

    target_link_options(${SIMULATION_TARGET} PRIVATE
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

    set_target_properties(${SIMULATION_TARGET} PROPERTIES
        OUTPUT_NAME ${SIMULATION_RUNTIME_NAME}
        SUFFIX ".js"
        RUNTIME_OUTPUT_DIRECTORY
            "${CMAKE_CURRENT_SOURCE_DIR}/public/runtime/${SIMULATION_RUNTIME_NAME}"
    )
endfunction()

physics_lab_add_simulation(
    TARGET physics_lab_ballistics
    RUNTIME_NAME ballistics
    ENTRY_POINT web/ballistics/main.cpp
    PROJECT_DIRECTORY project1_balistica
    NATIVE_SOURCES ${BALLISTICS_NATIVE_SOURCES}
)

physics_lab_add_simulation(
    TARGET physics_lab_pendulum
    RUNTIME_NAME pendulum
    ENTRY_POINT web/pendulum/main.cpp
    PROJECT_DIRECTORY project2_pendulum
    NATIVE_SOURCES ${PENDULUM_NATIVE_SOURCES}
)

physics_lab_add_simulation(
    TARGET physics_lab_gas_brownian
    RUNTIME_NAME gas-brownian
    ENTRY_POINT web/gas-brownian/main.cpp
    PROJECT_DIRECTORY project3_gas_brownian
    NATIVE_SOURCES ${GAS_BROWNIAN_NATIVE_SOURCES}
)
