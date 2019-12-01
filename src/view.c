#include <assert.h>
#include <stdlib.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "errors.h"
#include "gl.h"
#include "matrix.h"
#include "terrain.h"
#include "text.h"
#include "view.h"

typedef struct {
    GLuint vao;
    GLuint positions;
    GLuint colors;
} Axis;

static void Axis_Init(Axis *axis, const vec3 *v, const vec3 *color)
{
    vec3 positions[2] = {0};
    vec3 colors[2] = { *color, *color };
    vec3_Scale(1000, v, &positions[1]);

    // Create VAO
    glGenVertexArrays(1, &axis->vao);
    glBindVertexArray(axis->vao);
    {
        // Initialize position buffer
        glGenBuffers(1, &axis->positions);
        glBindBuffer(GL_ARRAY_BUFFER, axis->positions);
        {
            glBufferData(
                GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);
            glVertexAttribPointer(
                VERTEX_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(VERTEX_ATTRIB_POSITION);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Initialize color buffer
        glGenBuffers(1, &axis->colors);
        glBindBuffer(GL_ARRAY_BUFFER, axis->colors);
        {
            glBufferData(
                GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
            glVertexAttribPointer(
                VERTEX_ATTRIB_COLOR, 3, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(VERTEX_ATTRIB_COLOR);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);

    }
    glBindVertexArray(0);
}

static void Axis_Render(const Axis *axis)
{
    glBindVertexArray(axis->vao);
    {
        glDrawArrays(GL_LINES, 0, 2);
    }
    glBindVertexArray(0);
}

struct View {
    GLFWwindow *window;
    const Terrain *terrain;
    uint32_t num_vertices;
    Console console;

    mat4 projection;
        // Perspective projection matrix for the scene. Converts camera
        // coordinates to screen coordinates. This is initialized once and never
        // changes.
    mat4 view_projection;
        // `projection` right-multiplied by a view matrix which converts world
        // coordinates to camera coordinates. Overall, this matrix converts
        // world coordinates to screen coordinates. This matrix needs to be
        // recomputed whenever the camera changes position or orientation.
        //
        // Right-multiply this matrix by a model matrix to map model coordinates
        // to screen coordinates.

    // Terrain GL objects
    bool show_terrain;
    GLuint gl_terrain_vao;          // Vertex array object
    GLuint gl_terrain_positions;    // Position buffer
    GLuint gl_terrain_shaders;      // Shader program
    GLuint gl_terrain_shader_mvp;   // MVP matrix

    // Axis GL objects
    bool show_axes;
    Axis x_axis;
    Axis y_axis;
    Axis z_axis;
    GLuint gl_axis_shaders;
    GLuint gl_axis_shader_mvp;
};

static Command view_program;

View *View_New(GLFWwindow *window, const Terrain *terrain)
{
    View *view = Malloc(sizeof(View));

    view->window = window;
    view->terrain = terrain;
    view->show_terrain = true;
    view->show_axes = true;

    ////////////////////////////////////////////////////////////////////////////
    // Initialize view matrices
    //

    // Initialize the perspective projection.
    int window_width, window_height;
    glfwGetWindowSize(window, &window_width, &window_height);
    mat4_Perspective(&view->projection,
        M_PI/3,
            // pi/3, or 60 degree, field of vision.
        (float)window_width/window_height,
            // Aspect ratio is determined by window size.
        1.0, 1000.0
            // We use 1-yard for the near clipping plane and 1000y for the far.
    );

    // Initialize camera position:                                            //
    //                                                                        //
    // Top view:                                                              //
    //                                                                        //
    //              -------------           y                                 //
    //              |           |           ^                                 //
    //              |  Terrain  |           |                                 //
    //              |           |          z.--> x                            //
    //              |           |                                             //
    //              -------------                                             //
    //            ,/                                                          //
    //           /                                                            //
    //         CAM                                                            //
    //                                                                        //
    // Side view:                                                             //
    //                                                                        //
    //      CAM                             z                                 //
    //         \                            ^                                 //
    //           \                          |                                 //
    //             ---------------------   yo--> x                            //
    //                    Terrain                                             //
    //
    // Initialize with identity matrix.
    mat4_Copy(&view->view_projection, &I4);

    // First rotate to an isometric perspective
    mat4 m;
    mat4_Rotation(&m, M_PI/4, &z3);
    mat4_ComposeInPlace(&m, &view->view_projection);
        // Rotating the world 45 degrees (pi/4 radians) about the z-axis
        // effectively rotates the camera -45 degrees about the same axis,
        // so it points diagonally out from the origin, bisecting the angle
        // between the x and y axes.
    mat4_Rotation(&m, -M_PI/4, &x3);
    mat4_ComposeInPlace(&m, &view->view_projection);
        // Rotating the world -45 degrees (-pi/4 radians) about the x axis
        // effectively rotates the camera 45 degrees about the same axis, so it
        // points down towards the terrain but no longer straight down.
    vec3 zoom_out = {0, 0, -100};
    mat4_Translation(&m, &zoom_out);
    mat4_ComposeInPlace(&m, &view->view_projection);
        // Now the z-axis angles isometrically away from the terrain. We move
        // the terrain 100yds further down the z axis, effectively zooming out
        // 100yds. Now we're in camera coordinates.
    mat4_ComposeInPlace(&view->projection, &view->view_projection);
        // Apply the perspective projection, so `view_projection` now maps from
        // world space to screen space.

    ////////////////////////////////////////////////////////////////////////////
    // Initialize terrain data
    //

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
    glGenVertexArrays(1, &view->gl_terrain_vao);
    glBindVertexArray(view->gl_terrain_vao);
    {

        // Initialize vertex buffer
        glGenBuffers(1, &view->gl_terrain_positions);
        glBindBuffer(GL_ARRAY_BUFFER, view->gl_terrain_positions);
        {
            glBufferData(
                GL_ARRAY_BUFFER,
                sizeof(vec3)*view->num_vertices,
                positions,
                GL_DYNAMIC_DRAW
            );
            glVertexAttribPointer(
                VERTEX_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(VERTEX_ATTRIB_POSITION);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    glBindVertexArray(0);
    free(positions);
        // GL has copied the vertex data into GPU memory, so we can free our
        // buffer.

    ////////////////////////////////////////////////////////////////////////////
    // Initialize axis data
    //

    Axis_Init(&view->x_axis, &x3, &RGB_RED);
    Axis_Init(&view->y_axis, &y3, &RGB_GREEN);
    Axis_Init(&view->z_axis, &z3, &RGB_BLUE);

    ////////////////////////////////////////////////////////////////////////////
    // Load shader programs
    //

    view->gl_terrain_shaders = GL_LoadShaders(
        "shaders/terrain_vertex.glsl", "shaders/terrain_fragment.glsl");
    view->gl_terrain_shader_mvp = glGetUniformLocation(
        view->gl_terrain_shaders, "mvp");
    view->gl_axis_shaders = GL_LoadShaders(
        "shaders/axis_vertex.glsl", "shaders/axis_fragment.glsl");
    view->gl_axis_shader_mvp = glGetUniformLocation(
        view->gl_axis_shaders, "mvp");

    ////////////////////////////////////////////////////////////////////////////
    // Initialize console
    //

    Console_Init(&view->console,
        window,
        0,                      // x            (pixels)
        window_height,          // y            (pixels)
        80,                     // width        (columns)
        window_height/15,       // height       (rows)
        15,                     // font size    (height in pixels)
        &view_program, view);

    ////////////////////////////////////////////////////////////////////////////
    // Initialization complete
    //

    trace("Initialized view:\n", 0);
    trace("    projection:\n"
          "%s\n",
          mat4_String(&view->projection)
    );
    trace("    view_projection:\n"
          "%s\n",
          mat4_String(&view->view_projection)
    );

    return view;
}

void View_Render(View *view)
{
    Console_Render(&view->console);

    if (view->show_terrain) {
        // Draw terrain
        glUseProgram(view->gl_terrain_shaders);
        glUniformMatrix4fv(
            view->gl_terrain_shader_mvp, 1, GL_TRUE,
            mat4_Buffer(&view->view_projection)
        );

        glBindVertexArray(view->gl_terrain_vao);
        {
            glDrawArrays(GL_LINES, 0, view->num_vertices);
        }
        glBindVertexArray(0);
    }

    if (view->show_axes) {
        // Draw axes
        glUseProgram(view->gl_axis_shaders);
        glUniformMatrix4fv(
            view->gl_axis_shader_mvp, 1, GL_TRUE,
            mat4_Buffer(&view->view_projection)
        );

        Axis_Render(&view->x_axis);
        Axis_Render(&view->y_axis);
        Axis_Render(&view->z_axis);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Console program
//

#define PROGRAM_INFO(info) \
    info(PROG_ID, view_program) \
    info(PROG_STATE_TYPE, View) \
    info(PROG_STATE_NAME, view)

DECLARE_RUNNABLE(show_axes, "axes", "enable rendering of X, Y, and Z axes")
{
    (void)console;
    (void)argc;
    (void)argv;

    view->show_axes = true;
}

DECLARE_RUNNABLE(show_terrain, "terrain", "enable rendering of terrain mesh")
{
    (void)console;
    (void)argc;
    (void)argv;

    view->show_terrain = true;
}

DECLARE_SUB_COMMANDS(show, "show", "enable rendering of scene entities",
    &show_axes, &show_terrain);

DECLARE_RUNNABLE(hide_axes, "axes", "disable rendering of X, Y, and Z axes")
{
    (void)console;
    (void)argc;
    (void)argv;

    view->show_axes = false;
}

DECLARE_RUNNABLE(hide_terrain, "terrain", "disable rendering of terrain mesh")
{
    (void)console;
    (void)argc;
    (void)argv;

    view->show_terrain = false;
}

DECLARE_SUB_COMMANDS(hide, "hide", "disable rendering of scene entities",
    &hide_axes, &hide_terrain);

DECLARE_PROGRAM(&show, &hide);

#undef PROGRAM_INFO
