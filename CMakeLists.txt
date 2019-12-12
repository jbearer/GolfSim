cmake_minimum_required(VERSION 3.0.0)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find OpenGL.
find_package(OpenGL REQUIRED)

# Build OpenGL support libraries.
add_subdirectory(external)
include_directories(
    external/glfw-3.1.2/include/
    external/glew-1.13.0/include/
)
set(GL_LIBS
    ${OPENGL_LIBRARY}
    glfw
    GLEW_1130
)

if (CMAKE_BUILD_TYPE EQUAL Debug)
    # Build docs
    find_package(Doxygen)
    if (DOXYGEN_FOUND)
        message(INFO "Found Doxygen: ${DOXYGEN_EXECUTABLE}")
    else()
        message(WARNING "Doxygen not found, will not build docs")
    endif()
endif()

set(CMAKE_C_FLAGS "-Wall -Wextra -Werror -ansi -pedantic -std=c99")

if (CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGOLF_OS_LINUX")
elseif(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGOLF_OS_MAC")
elseif(CMAKE_SYSTEM_NAME STREQUAL Windows)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGOLF_OS_WINDOWS")
else()
    message(WARNING "Unrecognized operating system ${CMAKE_SYSTEM_NAME}")
endif()

add_subdirectory(src)
add_subdirectory(app)
