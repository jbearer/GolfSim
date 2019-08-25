#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GL/glew.h>

#include "errors.h"
#include "gl.h"

static void GL_CompileShader(GLuint shader, const char *source_path)
{
    FILE *file = fopen(source_path, "r");
    if (file == NULL) {
        Error_Raise(FATAL, ERR_IO, strerror(errno));
    }

    // Find the size of the file.
    fseek(file, 0, SEEK_END);
    GLint file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *source = Malloc(file_size);
    fread(source, 1, file_size, file);
    fclose(file);

    // Compile the shader
    glShaderSource(shader, 1, (const char * const *)&source, &file_size);
    glCompileShader(shader);
    free(source);

    // Check compilation
    GLint result = GL_FALSE;
    int err_length;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &err_length);
    if (err_length > 0) {
        char *err_msg = Malloc(err_length + 1);
        glGetShaderInfoLog(shader, err_length, NULL, err_msg);
        Error_Raise(FATAL, ERR_INVALID_SHADER, err_msg);
    }
}

GLuint GL_LoadShaders(const char *vertex_path, const char *fragment_path)
{

    // Create the shaders
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    // Compile them
    GL_CompileShader(vertex_shader, vertex_path);
    GL_CompileShader(fragment_shader, fragment_path);

    // Link the program
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    // Check the program
    GLint result = GL_FALSE;
    int err_length;
    glGetProgramiv(program, GL_LINK_STATUS, &result);
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &err_length);
    if (err_length > 0) {
        char *err_msg = Malloc(err_length + 1);
        glGetProgramInfoLog(program, err_length, NULL, err_msg);
        Error_Raise(FATAL, ERR_INVALID_SHADER, err_msg);
    }

    // Clean up
    glDetachShader(program, vertex_shader);
    glDetachShader(program, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return program;
}
