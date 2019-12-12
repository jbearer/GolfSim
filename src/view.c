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

#define CAMERA_PAN_YDS_MS 0.02
    // How fast the camera should pan when the user's mouse is positioned on an
    // edge of the window, in yds/ms.

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
    float camera_x;
    float camera_y;

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
    bool show_terrain_mesh;
    GLuint gl_terrain_vao;          // Vertex array object
    GLuint gl_terrain_positions;    // Position buffer
    GLuint gl_terrain_shaders;      // Shader program
    GLuint gl_terrain_shader_mvp;   // MVP matrix
    GLuint gl_terrain_shader_mesh;  // Mesh flag

    // Axis GL objects
    bool show_axes;
    Axis x_axis;
    Axis y_axis;
    Axis z_axis;
    GLuint gl_axis_shaders;
    GLuint gl_axis_shader_mvp;

#ifndef NDEBUG
    uint32_t dts[10];
        // Milliseconds required to render the last 10 frames. This array
        // represents a circular buffer.
    uint8_t dts_next;
        // Offset into `dts` of the next sample to overwrite.
#endif
};

static Command view_program;

static void View_UpdateMVP(View *view)
{
    // Initialize the perspective projection.
    int window_width, window_height;
    glfwGetWindowSize(view->window, &window_width, &window_height);
    mat4_Perspective(&view->projection,
        M_PI/3,
            // pi/3, or 60 degree, field of vision.
        (float)window_width/window_height,
            // Aspect ratio is determined by window size.
        10.0, 1000.0
            // We use 10 yards for the near clipping plane and 1000 for the far.
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

    mat4 m;
    vec3 v;

    // Move the camera to it's XY position. We will change the Z-coordinate when
    // we zoom out later, but it is easiest to do this now when we are level
    // with the terrain and not at any kind of angle.
    v = (vec3){-view->camera_x, -view->camera_y, 0};
    mat4_Translation(&m, &v);
    mat4_ComposeInPlace(&m, &view->view_projection);
        // We shift by -x and -y because we are actually keeping the camera
        // fixed and moving the world, so to give the appearence of moving the
        // camera one way we have to move the world the other.


    // Rotate to an isometric perspective
    mat4_Rotation(&m, M_PI/4, &z3);
    mat4_ComposeInPlace(&m, &view->view_projection);
        // Rotating the world 45 degrees (pi/4 radians) about the z-axis
        // effectively rotates the camera -45 degrees about the same axis,
        // so it points diagonally out from the origin, bisecting the angle
        // between the x and y axes:
        //                                                                    //
        //                         ,^.                                        //
        //                       ,`   ',                                      //
        //                     ,`   N   ',                                    //
        //          Y        ,`     ^     ',        X                         //
        //           `,    ,`       |       ',    ,`                          //
        //             `\,`    W<--CAM-->E    ` /                             //
        //               `,       ,`| `,      ,`                              //
        //                 `,   ,`  V   `,  ,`                                //
        //                   `/`    S     ,\                                  //
        //                 ,`  `,       ,`   `,                               //
        //    view->camera_y     `,   ,`       `view->camera_x                //
        //                         `V`                                        //
        //                                                                    //
        // Now that our perspective is skewed, we have two coordinate systems
        // that are useful to us: world coordinates and camera coordinates.
        // World coordinates are useful because they align with the rectangular
        // shape of the terrain, and the correspond to face and vertex indices
        // in the terrain model (with a possible scalar). Camera coordinates are
        // useful for describing things like camera orientation and motion.
        //
        // To disambiguate between these two coordinate systems, we will use the
        // standard XYZ Cartesian coordinates to talk about world coordinates,
        // and we will use compass directions to talk about camera coordinates,
        // as illustrated in the diagram above.
    mat4_Rotation(&m, -M_PI/4, &x3);
    mat4_ComposeInPlace(&m, &view->view_projection);
        // Rotating the world -45 degrees (-pi/4 radians) about the x axis
        // effectively rotates the camera 45 degrees about the same axis, so it
        // points down towards the terrain but no longer straight down.
    v = (vec3){0, 0, -30};
    mat4_Translation(&m, &v);
    mat4_ComposeInPlace(&m, &view->view_projection);
        // Now the z-axis angles isometrically away from the terrain. We move
        // the terrain 30yds further down the z axis, effectively zooming out
        // 30yds. Now we're in camera coordinates.
    mat4_ComposeInPlace(&view->projection, &view->view_projection);
        // Apply the perspective projection, so `view_projection` now maps from
        // world space to screen space.

    // Send the MVP matrix to the shaders.
    glUseProgram(view->gl_terrain_shaders);
    {
        glUniformMatrix4fv(
            view->gl_terrain_shader_mvp, 1, GL_TRUE,
            mat4_Buffer(&view->view_projection)
        );
    }
    glUseProgram(0);

    glUseProgram(view->gl_axis_shaders);
    {
        glUniformMatrix4fv(
            view->gl_axis_shader_mvp, 1, GL_TRUE,
            mat4_Buffer(&view->view_projection)
        );
    }
    glUseProgram(0);
}

View *View_New(GLFWwindow *window, const Terrain *terrain)
{
    View *view = Malloc(sizeof(View));

    view->window = window;
    view->terrain = terrain;
    view->show_terrain = true;
    view->show_terrain_mesh = false;
    view->show_axes = true;
    view->camera_x = 0;
    view->camera_y = 0;

#ifndef NDEBUG
    memset(view->dts, 0, sizeof(view->dts));
    view->dts_next = 0;
#endif

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

            const Face *face = Terrain_GetConstFace(terrain, row, col);
            const uint16_t *z = face->vertices;


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
            positions[i++] = (vec3){ row,   col,   z[0] };
            positions[i++] = (vec3){ row,   col+1, z[1] };
            positions[i++] = (vec3){ row+1, col,   z[3] };

            // Triangle B: (row+1, col+1), (row, col+1), (row+1, col)
            positions[i++] = (vec3){ row+1, col+1, z[2] };
            positions[i++] = (vec3){ row,   col+1, z[1] };
            positions[i++] = (vec3){ row+1, col,   z[3] };
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

    // Terrain shader
    view->gl_terrain_shaders = GL_LoadShaders(
        "shaders/terrain_vertex.glsl", "shaders/terrain_fragment.glsl");
    view->gl_terrain_shader_mvp = glGetUniformLocation(
        view->gl_terrain_shaders, "mvp");
    view->gl_terrain_shader_mesh = glGetUniformLocation(
        view->gl_terrain_shaders, "mesh");

    // Axis shader
    view->gl_axis_shaders = GL_LoadShaders(
        "shaders/axis_vertex.glsl", "shaders/axis_fragment.glsl");
    view->gl_axis_shader_mvp = glGetUniformLocation(
        view->gl_axis_shaders, "mvp");

    ////////////////////////////////////////////////////////////////////////////
    // Initialize view matrices
    //
    View_UpdateMVP(view);

    ////////////////////////////////////////////////////////////////////////////
    // Initialize console
    //

    int window_width, window_height;
    glfwGetWindowSize(view->window, &window_width, &window_height);
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

// Convert a vector in camera cooridnates (EN) to one in world coordinates (XY).
static inline vec2 View_ENtoXY(vec2 en)
{
    // We want to convert deltas in camera coordinates (the north and east
    // directions) to deltas in the x and y directions. To convert, we'll first
    // make the observation that we don't need to do anything special to handle
    // movement along the north and east axes simultaneously, because we can
    // just move north and then east in two steps -- walking 4 steps north and
    // then 3 steps east is the same as walking 5 steps in a north-northeasterly
    // direction:
    //
    //                                  3
    //                                .----B
    //                                |   /
    //                              4 |  / 5
    //                                | /
    //                                A`
    //
    // With that in mind, we will first figure out how to convert from a
    // northerly delta to X and Y deltas, and then we'll do the same for east.
    // Here's a picture showing both coordinate systems:
    //                                                                        //
    //                                 ,^.                                    //
    //                               ,`   ',                                  //
    //                             ,`       ',                                //
    //                  Y        ,`           ',        X                     //
    //                   `,    ,`               ',    ,`                      //
    //                     `\,`                   ` /                         //
    //                       `,        CAM'       ,`                          //
    //                         `,       |       ,`                            //
    //                           `,  θ _|ΔN   ,`                              //
    //                             `, , |   ,`                                //
    //                       /,      `, | ,`\θ     ,\                         //
    //                         `,      CAM--|--  ,`                           //
    //                       Δyₙ `,            ,`Δxₙ                          //
    //                             `/        \`                               //
    //                                                                        //
    // (The angle θ here is the angle between the XY axes and the north-east
    // axes, which is always π/4 in our isometric projection).
    //
    // Using basic trig identities, we can see that
    //                      Δxₙ/ΔN = cos(π/2 - θ)
    //                      Δyₙ/ΔN = cos(θ)
    // so
    //                      Δxₙ    = ΔNcos(π/2 - π/4) = ΔN/√2
    //                      Δyₙ    = ΔNcos(π/4)       = ΔN/√2
    //
    // We use a similar tactic for the east delta:
    //                                                                        //
    //                                 ,^.                                    //
    //                               ,`   ',                                  //
    //                             ,`       ',                                //
    //                  Y        ,`           ',        X                     //
    //                   `,    ,`               ',    ,`                      //
    //                     `\,`                   ` /                         //
    //                       `,                   ,`                          //
    //                         `,       |       ,`                            //
    //                           `,  θ _|     ,`                              //
    //                             `, , |   ,`                                //
    //                               `, | ,`\θ                                //
    //                                 CAM--|---CAM'                          //
    //                                      ΔE     ,\                         //
    //                              /,           ,`                           //
    //                                `,       \`   Δxₑ                       //
    //                            -Δyₑ  `/                                    //
    //                                                                        //
    // Once again with basic trig identities, we get
    //                    Δxₑ/ΔE = cos(θ)
    //                    Δyₑ/ΔE = -sin(θ)
    // so
    //                    Δxₑ    =  ΔEcos(π/4) = ΔE/√2
    //                    Δyₑ    = -ΔEsin(π/4) = -ΔE/√2
    //
    // Putting together the deltas from the northerly move and the easterly
    // move, we get
    //                   Δx = Δxₙ + Δxₑ = 1/√2 (ΔN + ΔE)
    //                   Δy = Δyₙ + Δyₑ = 1/√2 (ΔN - ΔE)
    return (vec2){M_SQRT1_2*(en.y + en.x), M_SQRT1_2*(en.y - en.x)};
}

// Convert a vector in world coordinates (XY) to one in camera coordinates (EN).
static inline vec2 View_XYtoEN(vec2 xy)
{
    // In the comments in View_ENtoXY, we derive
    //                        x = 1/√2 (N + E)
    //                        y = 1/√2 (N - E)
    // Adding these two equations gives
    //                    x + y = N√2
    // or
    //                        N = 1/√2 (x + y)
    // and subtracting them gives
    //                    x - y = E√2
    // or
    //                        E = 1/√2 (x - y)
    return (vec2){M_SQRT1_2*(xy.x - xy.y), M_SQRT1_2*(xy.x + xy.y)};
}

// Move the camera north and east by the given deltas. `north` and `east` may
// be negative, to allow moving south and west, respectively.
static void View_MoveCamera(View *view, float north, float east)
{
    // We are given deltas in the north and east directions, but we need to
    // modify the camera's x and y coordinates, so we first convert to XY
    // deltas.
    vec2 xy = View_ENtoXY((vec2){east,north});

    // Now we can directly modify the camera cooridnates.
    view->camera_x += xy.x;
    view->camera_y += xy.y;

    // We've changed the camera position, which is one of the inputs to the MVP
    // matrix, so we have to update the matrix.
    View_UpdateMVP(view);
}

static void View_Animate(View *view, uint32_t dt)
{
#ifndef NDEBUG
    ////////////////////////////////////////////////////////////////////////////
    // Update the frame-rate buffer so we can compute a running average.
    //
    view->dts[view->dts_next++] = dt;
    if (view->dts_next >= sizeof(view->dts)/sizeof(view->dts[0])) {
        view->dts_next = 0;
    }
#endif

    ////////////////////////////////////////////////////////////////////////////
    // Animate camera movement based on cursor position.
    //

    // Get the position of the cursor.
    double cursor_x, cursor_y;
    glfwGetCursorPos(view->window, &cursor_x, &cursor_y);
    int x = floor(cursor_x);
    int y = floor(cursor_y);

    // Get the size of the window to compare with the cursor.
    int width, height;
    glfwGetWindowSize(view->window, &width, &height);

    // Compute how far we will move the camera in each direction.
    float delta = CAMERA_PAN_YDS_MS*dt;

    // Set up cardinal direction deltas based on cursor position.
    float north = 0;
    float east = 0;
    if (x <= 0) {
        // Left edge of the window.
        east = -delta;
    } else if (x >= width - 1) {
        // Right edge.
        east = delta;
    }

    if (y <= 0) {
        // Top edge (glfwGetCursorPos gives you the position relative to the top
        // left, so y == 0 is the top edge and y == height is the bottom edge).
        north = delta;
    } else if (y >= height - 1) {
        // Bottom edge.
        north = -delta;
    }

    // Compute the coordinates of the camera so we can check if this movement is
    // taking us outside the viewable terrain area so we can clip it.
    vec2 camera = View_XYtoEN((vec2){view->camera_x, view->camera_y});

    // Clip the northerly delta so that the camera doesn't slide so far north or
    // south that we lose sight of the terrain and get lost.
    vec2 north_corner = View_XYtoEN((vec2){
        Terrain_FaceWidth(view->terrain),
        Terrain_FaceHeight(view->terrain)
    });
    if (camera.y + north > north_corner.y) {
        // The north delta would take us north of the northernmost corner of the
        // terrain, so set it to take us exactly there instead.
        north = north_corner.y - camera.y;
    } else if (camera.y + north < 0) {
        // The north delta would take us south of the southernmost corner of the
        // terrain, so set it to take us exactly there instead.
        north = -camera.y;
    }

    // Clip the easterly delta in the same way.
    vec2 east_corner = View_XYtoEN((vec2){Terrain_FaceWidth(view->terrain), 0});
    vec2 west_corner = View_XYtoEN(
        (vec2){0, Terrain_FaceHeight(view->terrain)});
    if (camera.x + east > east_corner.x) {
        east = east_corner.x - camera.x;
    } else if (camera.x + east < west_corner.x) {
        east = west_corner.x - camera.x;
    }

    View_MoveCamera(view, north, east);
}

void View_Render(View *view, uint32_t dt)
{
    View_Animate(view, dt);

    Console_Render(&view->console);

    if (view->show_terrain) {
        // Draw terrain
        glUseProgram(view->gl_terrain_shaders);
        glUniform1ui(view->gl_terrain_shader_mesh, 0);

        glBindVertexArray(view->gl_terrain_vao);
        {
            glDrawArrays(GL_TRIANGLES, 0, view->num_vertices);
        }
        glBindVertexArray(0);
    }

    if (view->show_terrain_mesh) {
        // Draw terrain mesh
        glUseProgram(view->gl_terrain_shaders);
        glUniform1ui(view->gl_terrain_shader_mesh, 1);
        glBindVertexArray(view->gl_terrain_vao);
        {
            glDrawArrays(GL_LINES, 0, view->num_vertices);
        }
        glBindVertexArray(0);
    }

    if (view->show_axes) {
        // Draw axes
        glUseProgram(view->gl_axis_shaders);
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

////////////////////////////////////////////////////////////////////////////////
// Show
//

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

DECLARE_RUNNABLE(
    show_terrain_mesh, "terrain-mesh", "enable rendering of terrain mesh")
{
    (void)console;
    (void)argc;
    (void)argv;

    view->show_terrain_mesh = true;
}

DECLARE_SUB_COMMANDS(show, "show", "enable rendering of scene entities",
    &show_axes, &show_terrain, &show_terrain_mesh);

////////////////////////////////////////////////////////////////////////////////
// Hide
//

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

DECLARE_RUNNABLE(
    hide_terrain_mesh, "terrain-mesh", "disable rendering of terrain mesh")
{
    (void)console;
    (void)argc;
    (void)argv;

    view->show_terrain_mesh = false;
}

DECLARE_SUB_COMMANDS(hide, "hide", "disable rendering of scene entities",
    &hide_axes, &hide_terrain, &hide_terrain_mesh);

////////////////////////////////////////////////////////////////////////////////
// Info
//

DECLARE_RUNNABLE(info_camera, "camera",
    "print information about the position of the camera")
{
    (void)argc;
    (void)argv;

    TextField_Printf((TextField *)console,
        "Camera x-coordinate: %d\n", (int)floor(view->camera_x));
    TextField_Printf((TextField *)console,
        "Camera y-coordinate: %d\n", (int)floor(view->camera_y));

    // Convert x and y to north and east.
    vec2 en = View_XYtoEN((vec2){view->camera_x, view->camera_y});
    TextField_Printf((TextField *)console,
        "Camera N-coordinate: %d\n", (int)floor(en.y));
    TextField_Printf((TextField *)console,
        "Camera E-coordinate: %d\n", (int)floor(en.x));
}

DECLARE_RUNNABLE(info_window, "window",
    "print information about the the window")
{
    (void)argc;
    (void)argv;

    int width, height;
    glfwGetWindowSize(view->window, &width, &height);
    TextField_Printf((TextField *)console, "Window width:  %d\n", width);
    TextField_Printf((TextField *)console, "Window height: %d\n", height);

    double cursor_x, cursor_y;
    glfwGetCursorPos(view->window, &cursor_x, &cursor_y);
    TextField_Printf((TextField *)console,
        "Cursor x: %d\n", (int)floor(cursor_x));
    TextField_Printf((TextField *)console,
        "Cursor y: %d\n", (int)floor(cursor_y));
}

#ifndef NDEBUG
DECLARE_RUNNABLE(info_frame_rate, "frame-rate",
    "print an estimate of the current frame rate")
{
    (void)argc;
    (void)argv;

    float   avg_ms = 0;
    uint8_t n      = sizeof(view->dts)/sizeof(view->dts[0]);

    for (uint8_t i = 0; i < n; ++i) {
        avg_ms += view->dts[i];
    }
    avg_ms = avg_ms / n;

    float rate = 1000.0 / avg_ms;
    TextField_Printf((TextField *)console, "%.1f\n", rate);
}
#endif

#ifdef NDEBUG
DECLARE_SUB_COMMANDS(info, "info", "print information about scene entities",
    &info_camera, &info_window);
#else
DECLARE_SUB_COMMANDS(info, "info", "print information about scene entities",
    &info_camera, &info_window, &info_frame_rate);
#endif

////////////////////////////////////////////////////////////////////////////////
// Move
//

DECLARE_RUNNABLE(move_camera, "camera", "<north> <east>")
{
    if (argc != 2) {
        TextField_PutLine(
            (TextField *)console, "command 'move camera' takes two arguments");
        return;
    }

    int north = atoi(argv[0]);
    int east  = atoi(argv[1]);
    View_MoveCamera(view, north, east);
}

DECLARE_SUB_COMMANDS(move, "move", "translate scene entities", &move_camera);

DECLARE_PROGRAM(&show, &hide, &info, &move);

#undef PROGRAM_INFO
