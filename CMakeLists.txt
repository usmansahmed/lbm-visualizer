cmake_minimum_required(VERSION 3.24)

# Use CMake's modern MSVC debug-information abstraction.
cmake_policy(SET CMP0141 NEW)

# Store debug information inside object files (/Z7).
# This avoids the problematic per-library compiler PDB copy.
set(
    CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
    "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>"
)

# Must be set before CUDA is enabled by project().
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES 86)
endif()

project(
    LBMVisualizer
    VERSION 0.1.0
    LANGUAGES C CXX CUDA
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

include(FetchContent)

# ---------------------------
# GLFW
# ---------------------------

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glfw)

# ---------------------------
# GLM
# ---------------------------

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.3
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glm)

# ---------------------------
# TinyXML2
# ---------------------------

add_library(tinyxml2 STATIC
    external/tinyxml2/tinyxml2.cpp
)

target_include_directories(tinyxml2
    PUBLIC
        external/tinyxml2
)

target_compile_features(tinyxml2
    PUBLIC
        cxx_std_17
)

# ---------------------------
# System libraries
# ---------------------------

find_package(OpenGL REQUIRED)
find_package(CUDAToolkit REQUIRED)

# ---------------------------
# GLAD
# ---------------------------

add_library(glad STATIC
    external/glad/src/gl.c
)

target_include_directories(glad
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
)

# ---------------------------
# IMGUI
# ---------------------------
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp

    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui
    PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

target_link_libraries(imgui
    PUBLIC
        glfw
)

# ---------------------------
# Application
# ---------------------------

add_executable(lbm_visualizer
    src/main.cpp
    src/VtiReader.cpp
    src/SliceExtractor.cpp
    src/Shader.cpp
    src/ScalarTexture.cpp
    src/Renderer.cpp
    src/UserInterface.cpp
    src/Application.cpp
    include/SimulationFrame.hpp
    include/VisualizationState.hpp
)

target_include_directories(lbm_visualizer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(lbm_visualizer
    PRIVATE
        glad
        glfw
        OpenGL::GL
        CUDA::cudart
        glm::glm
		tinyxml2
        imgui
)

set_target_properties(lbm_visualizer PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
)

if(MSVC)
    target_compile_options(lbm_visualizer
        PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/W4>
    )
else()
    target_compile_options(lbm_visualizer
        PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>
    )
endif()