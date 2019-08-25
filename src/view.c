#include <assert.h>
#include <stdlib.h>

#include <GL/glew.h>

#include "errors.h"
#include "gl.h"
#include "matrix.h"
#include "terrain.h"
#include "view.h"

typedef enum {
    VERTEX_ATTRIB_POSITION
} VertexAttribute;

struct View {
    Terrain *terrain;
    uint32_t num_vertices;

    GLuint gl_vertices;
    GLuint gl_position_buffer;
    GLuint gl_terrain_shaders;
};

View *View_New(Terrain *terrain)
{
    View *view = Malloc(sizeof(View));

    view->terrain = terrain;

    // Create vertex data
    view->num_vertices = 6*Terrain_NumFaces(terrain);
        // Each square face consists of 2 triangles, so 6 vertices.
    vec3 *positions = Malloc(sizeof(vec3)*view->num_vertices);

    uint32_t i = 0; // Index of current vertex in `positions`.

    // Initialize x-y vertex positions, 2 triangles for each face. Unlike the z
    // coordinates, these will never change, so we only have to do this once.
    for (uint16_t row = 0; row < Terrain_FaceWidth(terrain); ++row) {
        for (uint16_t col = 0; col < Terrain_FaceHeight(terrain); ++col) {
            assert(i < view->num_vertices);

            // We will draw a square face using two triangles, like this:
            //
            //        col   col+1
            //         |      |
            //   row --+------+--
            //         | A  / |
            //         |   /  |
            //         |  /   |
            //         | /  B |
            // row+1 --+------+--
            //         |      |
            //

            // Triangle A: (row, col), (row, col+1), (row+1, col)
            positions[i++] = (vec3){ row,   col,   0 };
            positions[i++] = (vec3){ row,   col+1, 0 };
            positions[i++] = (vec3){ row+1, col,   0 };

            // Triangle B: (row+1, col+1), (row, col+1), (row+1, col)
            positions[i++] = (vec3){ row+1, col+1, 0 };
            positions[i++] = (vec3){ row,   col+1, 0 };
            positions[i++] = (vec3){ row+1, col,   0 };
        }
    }

    // Initialize vertex array
    glGenVertexArrays(1, &view->gl_vertices);
    glBindVertexArray(view->gl_vertices);

    // Initialize vertex buffer
    glGenBuffers(1, &view->gl_position_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, view->gl_position_buffer);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(vec3)*view->num_vertices,
        positions,
        GL_DYNAMIC_DRAW
    );

    // GL has copied the vertex data into GPU memory, so we can free our buffer.
    free(positions);

    // Load shaders
    view->gl_terrain_shaders = GL_LoadShaders(
        "shaders/terrain_vertex.glsl", "shaders/terrain_fragment.glsl");

    return view;
}

void View_Render(View *view)
{
    glUseProgram(view->gl_terrain_shaders);

    glEnableVertexAttribArray(VERTEX_ATTRIB_POSITION);
    glBindBuffer(GL_ARRAY_BUFFER, view->gl_position_buffer);
    glVertexAttribPointer(
        VERTEX_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLES, 0, view->num_vertices);
    glDisableVertexAttribArray(VERTEX_ATTRIB_POSITION);
}
