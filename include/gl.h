/**
 * \file gl.h
 *
 * Utilities for working with GL at a slightly higher level.
 */

#ifndef GOLF_GL_H
#define GOLF_GL_H

#include <GL/glew.h>

#include "matrix.h"

typedef enum {
    VERTEX_ATTRIB_POSITION = 0,
    VERTEX_ATTRIB_COLOR = 1,
    VERTEX_ATTRIB_TEXTURE_UV = 2,
    VERTEX_ATTRIB_CURSOR = 3,
    VERTEX_ATTRIB_NORMAL = 4,
} VertexAttribute;

/**
 * \brief Compile and link a GLSL shader program.
 *
 * \param vertex_path Path to the source file for the vertex shader.
 * \param fragment_path Path to the source file for the fragment shader.
 *
 * \details This function performs several tasks:
 *  * Reads the GLSL source code from `vertex_path` and `fragment_path` into
 *    memory.
 *  * Compiles both programs, raising a `FATAL` error if compilation fails.
 *  * Links the two shaders into a single GLSL program, raising a `FATAL` error
 *    if linking fails.
 *  * Cleans up and returns a GL ID for the linked program.
 *
 *  This function will always either succeed or raise a fatal error.
 *
 * \return ID for the compiled shader program.
 */
GLuint GL_LoadShaders(const char *vertex_path, const char *fragment_path);

GLuint GL_LoadTexture(const char *bmp_path);

static const vec3 RGB_RED = { 1, 0, 0 };
static const vec3 RGB_GREEN = { 0, 1, 0 };
static const vec3 RGB_BLUE = { 0, 0, 1 };

#endif
