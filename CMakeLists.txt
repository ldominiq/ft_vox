cmake_minimum_required(VERSION 3.22)
project(ft_vox LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)

# Glad
add_library(glad glad/src/glad.c)
target_include_directories(glad PUBLIC glad/include)
set_target_properties(glad PROPERTIES POSITION_INDEPENDENT_CODE ON)


# GLFW
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG latest
)
FetchContent_MakeAvailable(glfw)

# GLM
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG master
)
FetchContent_MakeAvailable(glm)

# FastNoise
FetchContent_Declare(
  fastnoise
  GIT_REPOSITORY https://github.com/Auburn/FastNoiseLite.git
  GIT_TAG master
)
FetchContent_MakeAvailable(fastnoise)

# stb_image
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
)
FetchContent_MakeAvailable(stb)

if(ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g -O1")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")
endif()

# imgui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG master
)
FetchContent_MakeAvailable(imgui)

file(GLOB SRC "src/*.cpp")

if(EXISTS ${CMAKE_SOURCE_DIR}/assets)
  file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/shaders)
  file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})
endif()

# ---------------------------------------------------------------------------
# ImGui integration
# ---------------------------------------------------------------------------
set(IMGUI_DIR   ${imgui_SOURCE_DIR})

set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Create a static library for ImGui
add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

# Link ImGui to GLFW (it needs GLFW for its backend)
# Also link against dl on Linux for dynamic loading (glad/glfw handle OpenGL loader)
target_link_libraries(imgui PUBLIC glfw ${CMAKE_DL_LIBS})

add_executable(${PROJECT_NAME} ${SRC}
        include/TerrainParams.hpp)

# Modified shader files are now always copied to build folder now when modified
file(GLOB_RECURSE SHADER_FILES
        "${CMAKE_SOURCE_DIR}/shaders/*.vert"
        "${CMAKE_SOURCE_DIR}/shaders/*.frag"
)

add_custom_target(update_shaders DEPENDS ${SHADER_FILES})

add_dependencies(${PROJECT_NAME} update_shaders)

add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/shaders_timestamp
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        ${CMAKE_BINARY_DIR}/shaders
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/shaders_timestamp
        DEPENDS ${SHADER_FILES}
        COMMENT "Force copy shaders if any were modified"
)

add_custom_target(copy_shaders ALL DEPENDS ${CMAKE_BINARY_DIR}/shaders_timestamp)
add_dependencies(${PROJECT_NAME} copy_shaders)


target_include_directories(${PROJECT_NAME} PRIVATE
  include
  ${glm_SOURCE_DIR}
  ${fastnoise_SOURCE_DIR}/Cpp
  glad/include
  ${stb_SOURCE_DIR}
  ${imgui_SOURCE_DIR}
)

find_package(OpenGL REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
  glfw
  glad
  imgui
  OpenGL::GL
  dl
  pthread
  X11
  Xi
  Xrandr
  Xxf86vm
  z
)

target_compile_options(ft_vox PRIVATE -fPIE)
target_link_options(ft_vox PRIVATE -pie)
