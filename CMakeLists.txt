cmake_minimum_required(VERSION 3.24)
project(ft_vox)

set(CMAKE_CXX_STANDARD 23)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/Libraries/include)

link_directories(${CMAKE_SOURCE_DIR}/Libraries/lib)

# Add the executable and its source files
add_executable(ft_vox src/main.cpp external/glad/glad.cpp external/stb_image/stb_image.cpp)

# Link the GLAD and GLFW libraries
#target_link_libraries(ft_vox glfw3 glad)

# Find and link the OpenGL library
#find_package(glfw3 3.4 REQUIRED)

find_package(OpenGL REQUIRED)
target_link_libraries(ft_vox PUBLIC glfw3 assimp OpenGL::GL ${CMAKE_DL_LIBS})

# Copy a directory from the main directory to the build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders ${CMAKE_CURRENT_SOURCE_DIR}/Models ${CMAKE_CURRENT_SOURCE_DIR}/Textures ${CMAKE_CURRENT_SOURCE_DIR}/external/lib/assimp-vc143-mt.dll
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Debug)

