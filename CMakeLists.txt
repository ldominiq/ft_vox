cmake_minimum_required(VERSION 3.22)
project(ft_vox LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
include(FetchContent)

# Glad
add_library(glad glad/src/glad.c)
target_include_directories(glad PUBLIC glad/include)

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

file(GLOB SRC "src/*.cpp")

if(EXISTS ${CMAKE_SOURCE_DIR}/assets)
  file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/shaders)
  file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})
endif()

add_executable(${PROJECT_NAME} ${SRC})

target_include_directories(${PROJECT_NAME} PRIVATE
  include
  ${glm_SOURCE_DIR}
  ${fastnoise_SOURCE_DIR}/Cpp
  glad/include
  ${stb_SOURCE_DIR}
)

find_package(OpenGL REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
  glfw
  glad
  OpenGL::GL
  dl
  pthread
  X11
  Xi
  Xrandr
  Xxf86vm
  z
)
