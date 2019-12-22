#include <stdlib.h>
#include <strings.h>

#include <GL/glew.h>

#include "errors.h"
#include "gl.h"
#include "matrix.h"
#include "terrain.h"
#include "terrain_view.h"
#include "text.h"
#include "view.h"

#define CAMERA_PAN_YDS_PER_MS_ZOOM 0.00067
    // How many yards the camera should pan when the user's mouse is positioned
    // on an edge of the window, per millisecond of simulation time, per yard
    // that the camera is zoomed away from the terrain.
    //
    // Dividing by the zoom means that we pan slower when the camera is closer
    // to the terrain, which keeps the "apparent speed" of the pan relatively
    // constant.

#define CAMERA_ZOOM_RATIO 1.1
    // Ratio between two zoom levels which are separated by one click of the
    // user's mouse wheel. For example, 1.1 means we zoom in 10% each time the
    // user scrolls up.

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

typedef struct {
    enum {
        HUD_RAISE_FACE,
        HUD_RAISE_VERTEX,
        HUD_SET_MATERIAL,
        HUD_NONE,
    } selection;

    struct {
        const Material *material;
            // Only valid if `selection == HUD_SET_MATERIAL`.
    } data;
} HUD;

struct TerrainView {
    View view;
    Terrain *terrain;
    uint32_t num_vertices;
    float camera_x;
    float camera_y;
    uint16_t camera_zoom;
    vec3 mouse_position;
        // We cache the mouse position from the last time the terrain was
        // rendered, including the z-value from the depth buffer right after
        // drawing the terrain. This allows us to recover 3D world-space
        // coordinates for the corresponding point on the terrain without ray-
        // casting.
        //
        // It's important that we read the z-coordinate right after we draw the
        // terrain, because objects drawn later (such as axes, or the console)
        // might overwrite that pixel in the depth buffer.
        //
        // The point stored here is represented in normalized device
        // coordinates:
        //  * x ranges from -1 to 1, scaled for the width of the window.
        //  * y ranges from -1 to 1, scaled for the height of the window.
        //  * z ranges from -1 to 1, where -1 is the near clipping plane and 1
        //    is the far one.
        //
        // Note that it is possible the mouse is in a location which does not
        // correspond to any location in the terrain. This case can be
        // distinguished because the z-coordinate of the point will be exactly 1
        // (since a mouse position that misses the terrain will hit the far
        // clipping plane).
    vec3 ruler_start;
        // When the user left-clicks and drags with no special tool selected, we
        // will draw a ruler from where the first clicked to where the mouse is
        // now. This point represents the point on the terrain where they first
        // clicked.
    TextField *ruler_text;
        // While the user is still holding down the left mouse button during a
        // click-and-drag, this will be a pointer to a text field displaying the
        // length of the ruler.
        //
        // Otherwise, if no ruler is being drawn, this will be NULL.
    bool draw_ruler;
        // While the user is still holding down the left mouse button during a
        // click-and-drag, this will be set to indicate that `ruler_start` is
        // meaningful and we should draw a line from `ruler_start` to the
        // current mouse position.

    HUD hud;

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
    mat4 view_projection_inv;
        // Inverse of `view_projection`. Converts screen coordinates to world
        // coordinates. This matrix must be updated whenever `view_projection`
        // is updated.
        //
        // Left-multiply this matrix by the inverse of a model matrix to map
        // screen coordinates to model coordinates.

    // Terrain GL objects
    bool show_terrain;
    bool show_terrain_mesh;
    GLuint gl_terrain_vao;          // Vertex array object
    GLuint gl_terrain_positions;    // Position buffer
    GLuint gl_terrain_normals;      // Normal buffer
    GLuint gl_terrain_colors;       // Color buffer
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

    // Lines: ruler, hole maps, etc.
    GLuint gl_lines_shaders;
    GLuint gl_lines_shader_mvp;
    GLuint gl_ruler_vao;
    GLuint gl_ruler_buffer;
    bool show_holes;
    GLuint gl_holes_vao[18];
    GLuint gl_holes_buffers[18];
    TextField *hole_labels[18];

#ifndef NDEBUG
    uint32_t dts[10];
        // Milliseconds required to render the last 10 frames. This array
        // represents a circular buffer.
    uint8_t dts_next;
        // Offset into `dts` of the next sample to overwrite.
#endif
};

static Command view_program;

// Convert a vector in camera coordinates (EN) to one in world coordinates (XY).
static inline vec2 TerrainView_ENtoXY(vec2 en)
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
static inline vec2 TerrainView_XYtoEN(vec2 xy)
{
    // In the comments in TerrainView_ENtoXY, we derive
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

// Convert a vector in world coordinates to one in screen coordinates.
static vec2 TerrainView_XYtoScreen(const TerrainView *view, vec2 xy)
{
    // Convert to normalized device coordinates by applying the view-projection
    // matrix. This is the exact same transformation that we do for each terrain
    // vertex in the shader.
    vec4 p = { xy.x, xy.y, Terrain_SampleHeight(view->terrain, xy.x, xy.y), 1 };
    mat4_ApplyInPlace(&view->view_projection, &p);
    vec4_ScaleInPlace(1.0/p.w, &p);

    // `p` is now in normalized device coordinates, which means `p.x` and `p.y`
    // range from -1 to 1. We need to map this range to the width and height of
    // the window, respectively.
    uint32_t width, height;
    View_GetWindowSize((View *)view, &width, &height);
    return (vec2){ width*(p.x + 1)/2, height*(p.y + 1)/2 };
}

static void TerrainView_UpdateHoleLines(TerrainView *view)
{
    for (uint8_t i = 0; i < 18; ++i) {
        const Hole *hole = Terrain_GetConstHole(view->terrain, i);
        if (hole == NULL) {
            View_Detach((View *)view->hole_labels[i]);
            continue;
        }

        // Create a buffer of waypoints.
        vec3 points[4];
        for (uint8_t j = 0; j < hole->par - 1; ++j) {
            uint16_t row = hole->shot_points[j][0];
            uint16_t col = hole->shot_points[j][1];

            float x = col*view->terrain->xy_resolution +
                      view->terrain->xy_resolution/2;
            float y = row*view->terrain->xy_resolution +
                      view->terrain->xy_resolution/2;
            float z = Terrain_SampleHeight(view->terrain, x, y) + 1;
                // The line is drawn 1 yard above the ground, to ensure it
                // doesn't get depth-tested away.

            points[j] = (vec3){x, y, z};
        }

        // Give the buffer to OpenGL.
        glBindVertexArray(view->gl_holes_vao[i]);
        {
            glBindBuffer(GL_ARRAY_BUFFER, view->gl_holes_buffers[i]);
            {
                glBufferData(
                    GL_ARRAY_BUFFER,
                    sizeof(vec3)*hole->par - 1,
                    points,
                    GL_DYNAMIC_DRAW
                );
                glVertexAttribPointer(
                    VERTEX_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 0, 0);
                glEnableVertexAttribArray(VERTEX_ATTRIB_POSITION);
            }
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        glBindVertexArray(0);

        // Fix the hole's label, if necessary.
        if (view->show_holes) {
            vec2 hole_loc_world = { points[hole->par - 2].x
                                  , points[hole->par - 2].y };
            vec2 hole_loc_screen = TerrainView_XYtoScreen(view, hole_loc_world);

            TextField_SetLocation(
                view->hole_labels[i],
                hole_loc_screen.x + 5,
                hole_loc_screen.y + 10
            );
            View_Attach((View *)view->hole_labels[i], (View *)view);
        } else {
            View_Detach((View *)view->hole_labels[i]);
        }
    }
}

static void TerrainView_UpdateMVP(TerrainView *view)
{
    // Initialize the perspective projection.
    uint32_t window_width, window_height;
    View_GetWindowSize((View *)view, &window_width, &window_height);
    mat4_Perspective(&view->projection,
        M_PI/3,
            // pi/3, or 60 degree, field of vision.
        (float)window_width/window_height,
            // Aspect ratio is determined by window size.
        10.0, 5000.0
            // Distances in yards to the near and var clipping planes,
            // respectively.
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
    v = (vec3){0, 0, -view->camera_zoom};
    mat4_Translation(&m, &v);
    mat4_ComposeInPlace(&m, &view->view_projection);
        // Now the z-axis angles isometrically away from the terrain. We move
        // the terrain 30yds further down the z axis, effectively zooming out
        // 30yds. Now we're in camera coordinates.
    mat4_ComposeInPlace(&view->projection, &view->view_projection);
        // Apply the perspective projection, so `view_projection` now maps from
        // world space to screen space.

    // Compute the inverse.
    bool invertible = mat4_Invert(
        &view->view_projection, &view->view_projection_inv);
    ASSERT(invertible);

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

    glUseProgram(view->gl_lines_shaders);
    {
        glUniformMatrix4fv(
            view->gl_lines_shader_mvp, 1, GL_TRUE,
            mat4_Buffer(&view->view_projection)
        );
    }
    glUseProgram(0);

    TerrainView_UpdateHoleLines(view);
        // The hole lines depend on the MVP, because they use it to map world
        // coordinates to screen coordinates in order to position labels.
}

static void TerrainView_VertexNormal(
    const TerrainView *view, uint16_t row, uint16_t col, vec3 *n)
{
    const Terrain *terrain = view->terrain;
    ASSERT(row < Terrain_VertexHeight(terrain));
    ASSERT(col < Terrain_VertexWidth(terrain));

    // The normal of a vertex incident to four faces:
    //
    //                   col-1      col       col+1
    //               row+1 ----------^-----------
    //                     |         |          |
    //                     |   F1   E12   F2    |
    //                     |         |          |
    //               row   <---E41---*----E23--->
    //                     |         |          |
    //                     |   F4   E34   F3    |
    //                     |         |          |
    //               row-1 ----------V-----------
    //
    // is given by
    //                       N1 + N2 + N3 + N4
    //                     ---------------------
    //                      |N1 + N2 + N3 + N4|
    // where Ni is the normal vector for face Fi. The normal for a face is
    // obtained by taking the cross-product of two of the edges of that face.
    // For example,
    //                        N1 = E12 x E41
    //
    // There are a couple of subtleties here:
    //
    // First, note that the order in which we cross the edges to get the normal
    // for a face matters. For instance, E41 x E12 = -(E12 x E41) = -N1 != N1.
    // Since we are working in a right-handed coordinate system, we use the
    // right- hand rule to find the order in which to cross the edges. Since we
    // want our terrain to be facing up (that is, in the positive z- direction)
    // we cross the edges in order of a counter-clockwise traversal of the face
    // starting from our vertex of interest, and we always choose the two edges
    // which share that vertex. In the example of F1, we start from the point
    // marked start and proceed counter-clockwise, taking E12 as our first edge.
    // We skip the top edge and the left edge, since they do not share the
    // starred vertex, and then finally we reach E41 as our second edge.
    //
    // Second, you may notice that we may compute a different normal for the
    // same face depending on which edges we use to compute the normal for that
    // face (and, therefeore, depending on which incident vertex we are working
    // on). This happens because our faces are quadrilaterals (not triangles)
    // and thus the incident vertices may not all be coplanar, so it makes sense
    // that different vertices would observe different normals. The algorithm
    // outlined above for choosing edges uses the normal vector of the half-face
    // triangle closest to the center vertex, so when we compute the normal for
    // a vertex, we are really computing the normal for the inner area in this
    // picture:
    //
    //                     ----------^,----------
    //                     |     .,` | `.,      |
    //                     |  .,`    |    `.,   |
    //                     |,`       |       `. |
    //                     <---------*---------->
    //                     |`.,      |      .,` |
    //                     |   `.,   |   .,`    |
    //                     |      `. | ,`       |
    //                     ----------V-----------
    //
    // Only two of the four interior edges in the diagram above are actually
    // represented in the mesh (one parallel pair). The other two faces have an
    // interior edge which is perpendicular to the one shown. This means that
    // for non-planar faces, the shading induced by the normal vectors may
    // differ slightly from the actual shape of the mesh.
    //
    // We can fix this problem in the future by adding an extra vertex to the
    // center of every face, so that each faces looks like:
    //                 -----------
    //                 |`,.   .,`|
    //                 |   :,:   |
    //                 |.,`   `,.|
    //                 -----------
    //
    // This will allow us to both elevate and shade any vertex on the face
    // independently of the opposite vertex, and will make the face rotationally
    // symmetric, as opposed to the current faces which are biased in a
    // direction determined by their one interior edge.

    // Get a reference to each face incident to the current vertex, or NULL if
    // there is no such face (because the current vertex is up against an edge
    // of the terrain).
    bool at_top_edge    = row + 1 == Terrain_VertexHeight(terrain);
    bool at_bottom_edge = row     == 0;
    bool at_left_edge   = col     == 0;
    bool at_right_edge  = col + 1 == Terrain_VertexWidth(terrain);
    const Face *f1 = at_left_edge || at_top_edge ? NULL :
                        Terrain_GetConstFace(terrain, row, col - 1);
    const Face *f2 = at_right_edge || at_top_edge ? NULL :
                        Terrain_GetConstFace(terrain, row, col);
    const Face *f3 = at_right_edge || at_bottom_edge ? NULL :
                        Terrain_GetConstFace(terrain, row - 1, col);
    const Face *f4 = at_left_edge || at_bottom_edge ? NULL :
                        Terrain_GetConstFace(terrain, row - 1, col - 1);

    // Get the z-coordinate of the current vertex from one of the faces.
    ASSERT(f1 || f2 || f3 || f4);
        // In any non-empty terrain, every vertex touches at least one face.
    float z = f1 ? f1->vertices[BOTTOM_RIGHT]
            : f2 ? f2->vertices[BOTTOM_LEFT]
            : f3 ? f3->vertices[TOP_LEFT]
            :      f4->vertices[TOP_RIGHT];

    // Find the height of the vertex at the endpoint of each of the four edges.
    // We will need this information to compute the edges themselves. If in any
    // case there is no such vertex, we will use the height of the current
    // vertex. This is like surrounding the terrain with a hypothetical extra
    // row of vertices, which are each the same height as the vertex in the
    // terrain to which they are perpendicular.
    const float z12 = f1 ? f1->vertices[TOP_RIGHT]
                    : f2 ? f2->vertices[TOP_LEFT]
                    :      z;
    const float z23 = f2 ? f2->vertices[BOTTOM_RIGHT]
                    : f3 ? f3->vertices[TOP_RIGHT]
                    :      z;
    const float z34 = f3 ? f3->vertices[BOTTOM_LEFT]
                    : f4 ? f4->vertices[BOTTOM_RIGHT]
                    :      z;
    const float z41 = f4 ? f4->vertices[TOP_LEFT]
                    : f1 ? f1->vertices[BOTTOM_LEFT]
                    :      z;

    // Compute the edge vectors. Each edge has XY direction +-(0, 1) or +-(1,
    // 0), and the z-distance for each is obtained by subtracting the height of
    // the vertex at the end of the edge from the height of the current vertex
    // (z).
    const vec3 e12 = { 0,  1, z12 - z };
    const vec3 e23 = { 1,  0, z23 - z };
    const vec3 e34 = { 0, -1, z34 - z };
    const vec3 e41 = {-1,  0, z41 - z };

    // Compute the normal vectors for each face.
    vec3 n1; vec3_Cross(&e12, &e41, &n1);
    vec3 n2; vec3_Cross(&e23, &e12, &n2);
    vec3 n3; vec3_Cross(&e34, &e23, &n3);
    vec3 n4; vec3_Cross(&e41, &e34, &n4);

    // Add the vectors together to get the (unnormalized) normal.
    *n = (vec3){ 0, 0, 0 };
    vec3_AddInPlace(&n1, n);
    vec3_AddInPlace(&n2, n);
    vec3_AddInPlace(&n3, n);
    vec3_AddInPlace(&n4, n);

    // Normalize so the normal is a unit vector.
    vec3_NormalizeInPlace(n);
}

static void TerrainView_UpdateFaceHeights(TerrainView *view)
{
    vec3 *positions = Malloc(sizeof(vec3)*view->num_vertices);
    uint32_t i = 0; // Index of current vertex in `positions`.

    uint8_t w = view->terrain->xy_resolution;
    uint8_t h = view->terrain->xy_resolution;

    // Initialize x-y vertex positions, 2 triangles for each face.
    for (uint16_t row = 0; row < Terrain_FaceHeight(view->terrain); ++row) {
        for (uint16_t col = 0; col < Terrain_FaceWidth(view->terrain); ++col) {
            ASSERT(i < view->num_vertices);

            const Face *face = Terrain_GetConstFace(view->terrain, row, col);
            const uint16_t *z = face->vertices;


            // We will draw a square face using two triangles, like this:
            //
            //        col   col+1
            //         |      |
            // row+1 --+------+--
            //         | A  / |
            //         |   /  |
            //         |  /   |
            //         | /  B |
            // row   --+------+--
            //         |      |
            //

            // Triangle A
            positions[i++] = (vec3){ w*col,     h*(row+1), z[TOP_LEFT] };
            positions[i++] = (vec3){ w*col,     h*row,     z[BOTTOM_LEFT] };
            positions[i++] = (vec3){ w*(col+1), h*(row+1), z[TOP_RIGHT] };

            // Triangle B
            positions[i++] = (vec3){ w*(col+1), h*row,     z[BOTTOM_RIGHT] };
            positions[i++] = (vec3){ w*(col+1), h*(row+1), z[TOP_RIGHT] };
            positions[i++] = (vec3){ w*col,     h*row,     z[BOTTOM_LEFT] };
        }
    }

    // Copy data into OpenGL's vertex buffer.
    glBindVertexArray(view->gl_terrain_vao);
    {
        // Initialize vertex buffer
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

    // Initialize vertex normals.
    vec3 *normals = Malloc(sizeof(vec3)*view->num_vertices);
    i = 0;
    for (uint16_t row = 0; row < Terrain_FaceHeight(view->terrain); ++row) {
        for (uint16_t col = 0; col < Terrain_FaceWidth(view->terrain); ++col) {
            ASSERT(i < view->num_vertices);

            // Compute normals for each of the 6 vertices corresponding to the
            // 2 triangles for this face:
            //
            //        col   col+1
            //         |      |
            // row+1 --+------+--
            //         | A  / |
            //         |   /  |
            //         |  /   |
            //         | /  B |
            // row   --+------+--
            //         |      |
            //

            // Triangle A
            TerrainView_VertexNormal(view, row + 1, col,     &normals[i++]);
            TerrainView_VertexNormal(view, row,     col,     &normals[i++]);
            TerrainView_VertexNormal(view, row + 1, col + 1, &normals[i++]);

            // Triangle B
            TerrainView_VertexNormal(view, row,     col + 1, &normals[i++]);
            TerrainView_VertexNormal(view, row + 1, col + 1, &normals[i++]);
            TerrainView_VertexNormal(view, row,     col,     &normals[i++]);
        }
    }

    // Copy data into OpenGL's vertex buffer.
    glBindVertexArray(view->gl_terrain_vao);
    {
        // Initialize vertex buffer
        glBindBuffer(GL_ARRAY_BUFFER, view->gl_terrain_normals);
        {
            glBufferData(
                GL_ARRAY_BUFFER,
                sizeof(vec3)*view->num_vertices,
                normals,
                GL_DYNAMIC_DRAW
            );
            glVertexAttribPointer(
                VERTEX_ATTRIB_NORMAL, 3, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(VERTEX_ATTRIB_NORMAL);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    glBindVertexArray(0);
    free(normals);
        // GL has copied the vertex data into GPU memory, so we can free our
        // buffer.

    TerrainView_UpdateHoleLines(view);
        // The hole lines depend on the face heights, because we draw them at
        // the height of the shot-points.
}

static void TerrainView_UpdateFaceColors(TerrainView *view)
{
    vec4 *colors = Malloc(sizeof(vec4)*view->num_vertices);
    uint32_t i = 0; // Index of current vertex in `colors`.

    for (uint16_t row = 0; row < Terrain_FaceWidth(view->terrain); ++row) {
        for (uint16_t col = 0; col < Terrain_FaceHeight(view->terrain); ++col) {
            ASSERT(i < view->num_vertices);

            const Face *face = Terrain_GetConstFace(view->terrain, row, col);

            // Each face is composed of two triangles, which means it has 6
            // vertices. So the next 6 points in the vertex buffer correspond to
            // this face. We will make them all the color of the face.
            for (uint8_t j = 0; j < 6; ++j) {
                colors[i++] = face->material->color;
            }
        }
    }

    // Copy data into OpenGL's vertex buffer.
    glBindVertexArray(view->gl_terrain_vao);
    {
        glBindBuffer(GL_ARRAY_BUFFER, view->gl_terrain_colors);
        {
            glBufferData(
                GL_ARRAY_BUFFER,
                sizeof(vec4)*view->num_vertices,
                colors,
                GL_DYNAMIC_DRAW
            );
            glVertexAttribPointer(
                VERTEX_ATTRIB_COLOR, 4, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(VERTEX_ATTRIB_COLOR);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    glBindVertexArray(0);
    free(colors);
}

// Move the camera north and east by the given deltas. `north` and `east` may
// be negative, to allow moving south and west, respectively.
static void TerrainView_MoveCamera(TerrainView *view, float north, float east)
{
    // We are given deltas in the north and east directions, but we need to
    // modify the camera's x and y coordinates, so we first convert to XY
    // deltas.
    vec2 xy = TerrainView_ENtoXY((vec2){east,north});

    // Now we can directly modify the camera cooridnates.
    view->camera_x += xy.x;
    view->camera_y += xy.y;

    // We've changed the camera position, which is one of the inputs to the MVP
    // matrix, so we have to update the matrix.
    TerrainView_UpdateMVP(view);
}

static void TerrainView_HandleClick(
    View *view_base, MouseButton button, MouseAction action, ModifierKey mods)
{
    (void)button;
    (void)mods;

    TerrainView *view = (TerrainView *)view_base;

    trace("handling %s %s at {%.2f, %.2f, %.2f}\n",
        button == MOUSE_BUTTON_LEFT   ? "left"   :
        button == MOUSE_BUTTON_RIGHT  ? "right"  :
        button == MOUSE_BUTTON_MIDDLE ? "middle" :
                                        "unknown",
        action == MOUSE_PRESS   ? "click" :
        action == MOUSE_DRAG    ? "drag"  :
        action == MOUSE_RELEASE ? "release" :
                                  "unknown",
        view->mouse_position.x, view->mouse_position.y, view->mouse_position.z
    );

    vec4 p;
        // We will set `p` to the coordinates of the click in world space if the
        // click hits the terrain, or a sentinel {-1, -1, -1, 1} if not.
    if (view->mouse_position.z != 1) {
        // Convert from normalized device coordinates to world coordinates by
        // applying the inverse of the view-projection transformation.
        p = (vec4){ view->mouse_position.x
                  , view->mouse_position.y
                  , view->mouse_position.z
                  , 1 };
        mat4_ApplyInPlace(&view->view_projection_inv, &p);

        // The matrix transformation above leaves us in homogeneous coordinates.
        // To get back to Cartesian coordinates, we divide by `w`.
        vec4_ScaleInPlace(1/p.w, &p);
        trace("{%.2f, %.2f, %.2f} in screen space is "
              "{%.2f, %.2f, %.2f} in world space\n",
            view->mouse_position.x,
            view->mouse_position.y,
            view->mouse_position.z,
            p.x, p.y, p.z);

        // If we think we hit the terrain, we better have a point that
        // corresponds to some face.
        if (!(0 <= p.x && p.x <
                Terrain_FaceWidth(view->terrain)*view->terrain->xy_resolution)
            // X is out of range.
        ||
            !(0 <= p.y && p.y <
                Terrain_FaceHeight(view->terrain)*view->terrain->xy_resolution))
            // Y is out of range.
        {
            // We got a click with a non-1 depth value, but after the inverse
            // transformation it falls outside the perimeter of the terrain.
            // This can happen due to small numeric instabilities in the
            // coordinate transform when the click is actually inside the
            // terrain, but very near the edge.
            //
            // We could try to correct for this by nudging such problem points
            // back towards the edge of the terrain. However, the simpler
            // solution is to just drop the click when this happens, and in
            // practice it seems that these events are usually so close to the
            // edge of the terrain that the user won't notice anything odd when
            // they drop.
            p = (vec4){-1, -1, -1, 1};
        }
    } else {
        p = (vec4){-1, -1, -1, 1};
    }

    if (p.x < 0 && action != MOUSE_RELEASE) {
        return;
            // Don't initiate any events (MOUSE_PRESS or MOUSE_DRAG) for clicks
            // that missed the terrain. We will keep going if action is
            // MOUSE_RELEASE, so actions which were triggered by a click within
            // the terrain can complete even if the user has moved their mouse
            // outside the terrain by the time they release.
    }

    switch (view->hud.selection) {
        case HUD_RAISE_FACE: {
            if (action != MOUSE_PRESS) {
                break;
                    // Unlike some of the other tools, the raise/lower terrain
                    // tools are not idempotent within a face, so we don't
                    // activate them on a drag event.
            }

            // Find the coordinates of the face _containing_ the cursor.
            uint16_t row = floor(p.y/view->terrain->xy_resolution);
            uint16_t col = floor(p.x/view->terrain->xy_resolution);

            // Raise one unit on a left click, lower one unit on a right click.
            if (button == MOUSE_BUTTON_LEFT) {
                Terrain_RaiseFace(view->terrain, row, col, 1);
            } else if (button == MOUSE_BUTTON_RIGHT) {
                Terrain_RaiseFace(view->terrain, row, col, -1);
            }
            TerrainView_UpdateFaceHeights(view);

            break;
        }

        case HUD_RAISE_VERTEX: {
            if (action != MOUSE_PRESS) {
                break;
            }

            // Find the coordinates of the vertex _nearest_ the cursor.
            uint16_t row = round(p.y/view->terrain->xy_resolution);
            uint16_t col = round(p.x/view->terrain->xy_resolution);

            // Raise one unit on a left click, lower one unit on a right click.
            if (button == MOUSE_BUTTON_LEFT) {
                Terrain_RaiseVertex(view->terrain, row, col, 1);
            } else if (button == MOUSE_BUTTON_RIGHT) {
                Terrain_RaiseVertex(view->terrain, row, col, -1);
            }
            TerrainView_UpdateFaceHeights(view);

            break;
        }

        case HUD_SET_MATERIAL: {
            if (action != MOUSE_PRESS && action != MOUSE_DRAG) {
                break;
            }

            // Find the face _containing_ the cursor.
            uint16_t row = floor(p.y/view->terrain->xy_resolution);
            uint16_t col = floor(p.x/view->terrain->xy_resolution);

            if (button == MOUSE_BUTTON_LEFT) {
                // The material to set is stored in the HUD's data field.
                Terrain_GetFace(view->terrain, row, col)->material =
                    view->hud.data.material;
            } else {
                // Reset the face to rough.
                Terrain_GetFace(view->terrain, row, col)->material = &rough;
            }
            TerrainView_UpdateFaceColors(view);

            break;
        }

        case HUD_NONE: {
            if (button == MOUSE_BUTTON_LEFT) {
                if (action == MOUSE_PRESS) {
                    // Start drawing a ruler from this point.
                    view->ruler_start = (vec3){p.x, p.y, p.z + 1.0};
                        // Elevate the ruler 1 yard above the terrain to make
                        // sure it doesn't get depth-tested away.

                    int32_t mouse_x, mouse_y;
                    View_GetCursorPos((View *)view, &mouse_x, &mouse_y);

                    // Create a text field to display the length of the ruler.
                    ASSERT(view->ruler_text == NULL);
                    view->ruler_text = TextField_New(
                        sizeof(TextField), View_GetManager((View *)view),
                        (View *)view,           // parent
                        mouse_x, mouse_y - 5,   // top-left corner
                        5, 1,                   // cols x rows
                        15                      // font size
                    );
                    TextField_SetBackgroundColor(view->ruler_text, &RGBA_CLEAR);
                } else if (action == MOUSE_DRAG) {
                    if (view->ruler_text == NULL) {
                        ASSERT(!view->draw_ruler);
                        break;
                            // This case happens when we start a click off the
                            // terrain and then drag the mouse onto the terrain.
                            // It's reasonable to do nothing here.
                    }

                    // Draw a line from view->ruler_start to `p`.
                    vec3 ruler_end = {p.x, p.y, p.z + 1.0};
                        // Offset by one vertically so the ruler is
                        // drawn in front of the terrain.

                    glBindVertexArray(view->gl_ruler_vao);
                    {
                        glBindBuffer(GL_ARRAY_BUFFER, view->gl_ruler_buffer);
                        {
                            vec3 points[] = {view->ruler_start, ruler_end};
                            glBufferData(
                                GL_ARRAY_BUFFER,
                                sizeof(points),
                                points,
                                GL_DYNAMIC_DRAW
                            );
                            glVertexAttribPointer(
                                VERTEX_ATTRIB_POSITION,
                                3,
                                GL_FLOAT,
                                GL_FALSE,
                                0,
                                0
                            );
                            glEnableVertexAttribArray(VERTEX_ATTRIB_POSITION);
                        }
                        glBindBuffer(GL_ARRAY_BUFFER, 0);
                    }
                    glBindVertexArray(0);

                    view->draw_ruler = true;
                        // Ensure that the line actually gets drawn next frame.

                    // Update the text field with the new length of the ruler.
                    ASSERT(view->ruler_text != NULL);
                    vec3 ruler;
                    vec3_Subtract(&ruler_end, &view->ruler_start, &ruler);
                    TextField_Printf(view->ruler_text,
                        "%d     ", (int)round(vec3_Norm(&ruler)));
                            // Print the new length (rounded to the nearest
                            // integer) followed by enough whitespace to clear
                            // out the previous length. It's okay if we write
                            // too many whitespace characters; the text field
                            // will ignore them.
                    TextField_Flush(view->ruler_text);
                        // We have to flush since we didn't write a newline.
                    TextField_SetCursor(view->ruler_text, 0);
                        // Set the cursor back to the beginning of the line so
                        // we overwrite this update the next time we update the
                        // length.

                } else if (action == MOUSE_RELEASE) {
                    // Clear the ruler.
                    view->draw_ruler = false;

                    // Close the text field.
                    if (view->ruler_text != NULL) {
                        View_Close((View *)view->ruler_text);
                        view->ruler_text = NULL;
                    }
                }
            }

            break;
        }

        default:
            ASSERT(false);
    }
}

static void TerrainView_HandleScroll(View *view_base, int32_t x, int32_t y)
{
    trace("got scroll event (%d, %d)\n", x, y);

    TerrainView *view = (TerrainView *)view_base;

    (void)x;
        // Horizontal scrolling doesn't mean anything, at least for now.

    if (y > 0) {
        view->camera_zoom /= CAMERA_ZOOM_RATIO*y;
        if (view->camera_zoom < 1.0/(CAMERA_ZOOM_RATIO - 1)) {
            view->camera_zoom = 1.0/(CAMERA_ZOOM_RATIO - 1) + 1;
                // If we zoom in too far, we start to lose precision, to the
                // point where it becomes impossible to zoom out because
                // `camera_zoom*CAMERA_ZOOM_RATIO == camera_zoom` in integer
                // arithmetic.
        }
    } else if (y < 0) {
        view->camera_zoom *= -CAMERA_ZOOM_RATIO*y;
    }
    TerrainView_UpdateMVP(view);
}

static void TerrainView_Animate(TerrainView *view, uint32_t dt)
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
    int32_t x, y;
    View_GetCursorPos((View *)view, &x, &y);

    // Get the size of the window to compare with the cursor.
    uint32_t width, height;
    View_GetWindowSize((View *)view, &width, &height);

    // Compute how far we will move the camera in each direction.
    float delta = CAMERA_PAN_YDS_PER_MS_ZOOM*view->camera_zoom*dt;

    // Set up cardinal direction deltas based on cursor position.
    float north = 0;
    float east = 0;
    if (x <= 0) {
        // Left edge of the window.
        east = -delta;
    } else if (x >= (int32_t)width - 1) {
        // Right edge.
        east = delta;
    }

    if (y >= (int32_t)height - 1) {
        // Top edge.
        north = delta;
    } else if (y <= 0) {
        // (View_GetCursorPos gives you the position relative to the bottom
        // left, so y == 0 is the top edge and y == height is the bottom edge).
        north = -delta;
    }

    // Compute the coordinates of the camera so we can check if this movement is
    // taking us outside the viewable terrain area so we can clip it.
    vec2 camera = TerrainView_XYtoEN((vec2){view->camera_x, view->camera_y});

    // Clip the northerly delta so that the camera doesn't slide so far north or
    // south that we lose sight of the terrain and get lost.
    vec2 north_corner = TerrainView_XYtoEN((vec2){
        Terrain_FaceWidth(view->terrain)*view->terrain->xy_resolution,
        Terrain_FaceHeight(view->terrain)*view->terrain->xy_resolution
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
    vec2 east_corner = TerrainView_XYtoEN((vec2){
        Terrain_FaceWidth(view->terrain)*view->terrain->xy_resolution, 0});
    vec2 west_corner = TerrainView_XYtoEN((vec2){
        0, Terrain_FaceHeight(view->terrain)*view->terrain->xy_resolution});
    if (camera.x + east > east_corner.x) {
        east = east_corner.x - camera.x;
    } else if (camera.x + east < west_corner.x) {
        east = west_corner.x - camera.x;
    }

    TerrainView_MoveCamera(view, north, east);
}

static void TerrainView_Render(View *view_base, uint32_t dt)
{
    TerrainView *view = (TerrainView *)view_base;

    TerrainView_Animate(view, dt);

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

    // Cache the position of the mouse, including the z-coordinate from the
    // depth buffer. We have to do this immediately after drawing the terrain,
    // while the depth buffer values are guaranteed to correspond to the terrain
    // and not some other model.
    int32_t x, y;
    uint32_t width, height;
    GLfloat z;
    View_GetWindowSize((View *)view, &width, &height);
    View_GetCursorPos((View *)view, &x, &y);
    glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &z);
    view->mouse_position = (vec3){
        2*((float)x + 0.5)/width - 1,
            // We offset `x` by 0.5, because the integer point (x, y) is the
            // location of the bottom left corner of the pixel, but we really
            // want the point at the center of the pixel. Then we divide by
            // width to normalize to [0, 1], and then 2x + 1 stretches that
            // range to [-1, 1].
        2*((float)y + 0.5)/height - 1,
            // Same normalization as for `x`.
        2*z - 1
            // `z` is already normalized to [0, 1], so we don't have to adjust
            // for the size of the window or anything. We just take 2z - 1 to
            // scale this range to [-1, 1].
    };

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

    if (view->show_holes) {
        glUseProgram(view->gl_lines_shaders);
        for (uint8_t i = 0; i < 18; ++i) {
            const Hole *hole = Terrain_GetConstHole(view->terrain, i);
            if (hole == NULL) {
                continue;
            }

            glBindVertexArray(view->gl_holes_vao[i]);
            {
                glDrawArrays(GL_LINE_STRIP, 0, hole->par - 1);
            }
            glBindVertexArray(0);
        }
    }

    if (view->draw_ruler) {
        // Draw ruler
        glUseProgram(view->gl_lines_shaders);
        glBindVertexArray(view->gl_ruler_vao);
        {
            glDrawArrays(GL_LINES, 0, 2);
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

static void TerrainView_Destroy(View *view_base)
{
    TerrainView *view = (TerrainView *)view_base;

    // Close detached labels (which aren't children, and hence won't be
    // automatically closed).
    for (uint8_t i = 0; i < 18; ++i) {
        View *label = (View *)view->hole_labels[i];
        if (View_IsDetached(label)) {
            View_Close(label);
        }
    }
}

TerrainView *TerrainView_New(ViewManager *manager, Terrain *terrain)
{
    TerrainView *view = (TerrainView *)View_New(
        sizeof(TerrainView), manager, NULL);
    View_SetRenderCallback((View *)view, TerrainView_Render);
    View_SetMouseButtonCallback((View *)view, TerrainView_HandleClick);
    View_SetScrollCallback((View *)view, TerrainView_HandleScroll);
    View_SetDestroyCallback((View *)view, TerrainView_Destroy);

    view->terrain = terrain;
    view->show_terrain = true;
    view->show_terrain_mesh = false;
    view->show_axes = false;
    view->show_holes = false;
    view->camera_x = 0;
    view->camera_y = 0;
    view->camera_zoom = 300;
    view->mouse_position = (vec3){ 0, 0, 1 };
    view->hud.selection = HUD_NONE;
    view->ruler_start = (vec3){0, 0, 0};
    view->ruler_text = NULL;
    view->draw_ruler = false;

    for (uint8_t i = 0; i < 18; ++i) {
        view->hole_labels[i] = TextField_New(
            sizeof(TextField), manager, (View *)view, 0, 0, 2, 1, 15);
        TextField_SetBackgroundColor(view->hole_labels[i], &RGBA_CLEAR);
        TextField_SetForegroundColor(view->hole_labels[i], &RGBA_BLACK);
        TextField_Printf(view->hole_labels[i], "%d", i + 1);
        TextField_Flush(view->hole_labels[i]);
    }

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
    glGenVertexArrays(1, &view->gl_terrain_vao);
    glGenBuffers(1, &view->gl_terrain_positions);
    glGenBuffers(1, &view->gl_terrain_normals);
    TerrainView_UpdateFaceHeights(view);

    // Initialize color data
    glGenBuffers(1, &view->gl_terrain_colors);
    TerrainView_UpdateFaceColors(view);

    // Initialize lines
    glGenVertexArrays(1, &view->gl_ruler_vao);
    glGenBuffers(1, &view->gl_ruler_buffer);
    glGenVertexArrays(18, view->gl_holes_vao);
    glGenBuffers(18, view->gl_holes_buffers);

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

    // The light values are global constants that never change.
    glUseProgram(view->gl_terrain_shaders);
    {
        GLuint light_position = glGetUniformLocation(
            view->gl_terrain_shaders, "light_position");
        glUniform3f(light_position, -0.2, -0.1, 1.5);
            // We position the sun in quadrant 4, because that is also where the
            // camera is positioned, so terrain facing the user will be more
            // illuminated than terrain facing away.

        GLuint light_color = glGetUniformLocation(
            view->gl_terrain_shaders, "light_color");
        glUniform4f(light_color, 1, 1, 0.85, 1);
    }
    glUseProgram(0);

    // Axis shader
    view->gl_axis_shaders = GL_LoadShaders(
        "shaders/axis_vertex.glsl", "shaders/axis_fragment.glsl");
    view->gl_axis_shader_mvp = glGetUniformLocation(
        view->gl_axis_shaders, "mvp");

    // Lines shader
    view->gl_lines_shaders = GL_LoadShaders(
        "shaders/lines_vertex.glsl", "shaders/lines_fragment.glsl");
    view->gl_lines_shader_mvp = glGetUniformLocation(
        view->gl_lines_shaders, "mvp");

    ////////////////////////////////////////////////////////////////////////////
    // Initialize view matrices
    //
    TerrainView_UpdateMVP(view);

    ////////////////////////////////////////////////////////////////////////////
    // Initialize holes
    //
    TerrainView_UpdateHoleLines(view);

    ////////////////////////////////////////////////////////////////////////////
    // Initialize console
    //

    View_UseProgram((View *)view, &view_program, view);

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
    trace("    view_projection_inv:\n"
          "%s\n",
          mat4_String(&view->view_projection_inv)
    );

    return view;
}

////////////////////////////////////////////////////////////////////////////////
// Console program
//

#define PROGRAM_INFO(info) \
    info(PROG_ID, view_program) \
    info(PROG_STATE_TYPE, TerrainView) \
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

DECLARE_RUNNABLE(show_routing, "routing", "enable rendering of routing map")
{
    (void)console;
    (void)argc;
    (void)argv;

    view->show_holes = true;
    TerrainView_UpdateHoleLines(view);
}

DECLARE_SUB_COMMANDS(show, "show", "enable rendering of scene entities",
    &show_axes, &show_terrain, &show_terrain_mesh, &show_routing);

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

DECLARE_RUNNABLE(hide_routing, "routing", "disable rendering of routing map")
{
    (void)console;
    (void)argc;
    (void)argv;

    view->show_holes = false;
    TerrainView_UpdateHoleLines(view);
}

DECLARE_SUB_COMMANDS(hide, "hide", "disable rendering of scene entities",
    &hide_axes, &hide_terrain, &hide_terrain_mesh, &hide_routing);

////////////////////////////////////////////////////////////////////////////////
// Window
//

DECLARE_RUNNABLE(window_info, "info", "print information about the the window")
{
    (void)argc;
    (void)argv;

    // Print window size
    uint32_t width, height;
    View_GetWindowSize((View *)view, &width, &height);
    TextField_Printf((TextField *)console, "Window width:  %d\n", width);
    TextField_Printf((TextField *)console, "Window height: %d\n", height);

    // Print cursor position
    int32_t cursor_x, cursor_y;
    View_GetCursorPos((View *)view, &cursor_x, &cursor_y);
    TextField_Printf((TextField *)console,
        "Cursor x: %d\n", (int)floor(cursor_x));
    TextField_Printf((TextField *)console,
        "Cursor y: %d\n", (int)floor(cursor_y));

#ifndef NDEBUG
    // Print a running average of the frame rate
    float   avg_ms = 0;
    uint8_t n      = sizeof(view->dts)/sizeof(view->dts[0]);

    for (uint8_t i = 0; i < n; ++i) {
        avg_ms += view->dts[i];
    }
    avg_ms = avg_ms / n;

    float rate = 1000.0 / avg_ms;
    TextField_Printf((TextField *)console, "Frame rate: %.1f\n", rate);
#endif
}

DECLARE_SUB_COMMANDS(window, "window", "print information about the window",
    &window_info);

////////////////////////////////////////////////////////////////////////////////
// Camera
//

DECLARE_RUNNABLE(camera_move, "move", "<north> <east>")
{
    if (argc != 2) {
        TextField_PutLine(
            (TextField *)console, "command 'camera zoom' takes two arguments");
        return;
    }

    int north = atoi(argv[0]);
    int east  = atoi(argv[1]);
    TerrainView_MoveCamera(view, north, east);
}

DECLARE_RUNNABLE(camera_zoom, "zoom", "adjust the zoom by a delta")
{
    if (argc != 1) {
        TextField_PutLine(
            (TextField *)console, "command 'camera zoom' takes one argument");
        return;
    }

    int delta = atoi(argv[0]);
    view->camera_zoom -= delta;
    TerrainView_UpdateMVP(view);
}

DECLARE_RUNNABLE(camera_info, "info",
    "print information about the position of the camera")
{
    (void)argc;
    (void)argv;

    TextField_Printf((TextField *)console,
        "Camera x-coordinate: %d\n", (int)floor(view->camera_x));
    TextField_Printf((TextField *)console,
        "Camera y-coordinate: %d\n", (int)floor(view->camera_y));
    TextField_PutLine((TextField *)console, "");

    // Convert x and y to north and east.
    vec2 en = TerrainView_XYtoEN((vec2){view->camera_x, view->camera_y});
    TextField_Printf((TextField *)console,
        "Camera N-coordinate: %d\n", (int)floor(en.y));
    TextField_Printf((TextField *)console,
        "Camera E-coordinate: %d\n", (int)floor(en.x));
    TextField_PutLine((TextField *)console, "");

    TextField_Printf((TextField *)console,
        "Camera zoom: %d\n", (int)view->camera_zoom);
}

DECLARE_SUB_COMMANDS(camera, "camera", "inspect and manipulate the camera",
    &camera_move, &camera_zoom, &camera_info);

////////////////////////////////////////////////////////////////////////////////
// Terrain
//

static const Material *ParseMaterial(const char *name)
{
    if (strcasecmp("fairway", name) == 0) {
        return &fairway;
    } else if (strcasecmp("green", name) == 0) {
        return &green;
    } else if (strcasecmp("tee", name) == 0) {
        return &tee;
    } else if (strcasecmp("rough", name) == 0) {
        return &rough;
    } else if (strcasecmp("sand", name) == 0) {
        return &sand;
    } else if (strcasecmp("water", name) == 0) {
        return &water;
    } else {
        return NULL;
    }
}

DECLARE_RUNNABLE(terrain_set, "set",
    "set the material of face (<row>, <col>) to <material>")
{
    if (argc != 3) {
        TextField_PutLine((TextField *)console,
            "command 'terrain set' takes three arguments");
        return;
    }

    int row = atoi(argv[0]);
    int col = atoi(argv[1]);
    if (row < 0 || row >= Terrain_FaceHeight(view->terrain)) {
        TextField_PutLine((TextField *)console, "row out of range");
        return;
    }
    if (col < 0 || col >= Terrain_FaceHeight(view->terrain)) {
        TextField_PutLine((TextField *)console, "col out of range");
        return;
    }

    const Material *material = ParseMaterial(argv[2]);
    if (material == NULL) {
        TextField_PutLine((TextField *)console, "unrecognized material");
        return;
    }

    Terrain_GetFace(view->terrain, row, col)->material = material;
    TerrainView_UpdateFaceColors(view);
}

DECLARE_RUNNABLE(terrain_bulk_set, "bulk-set",
    "set the material of a rectangular region")
{
    if (argc != 5) {
        TextField_PutLine((TextField *)console,
            "command 'terrain bulk-set' takes five arguments");
        return;
    }

    int start_row = atoi(argv[0]);
    int start_col = atoi(argv[1]);
    int end_row   = atoi(argv[2]);
    int end_col   = atoi(argv[3]);

    if (start_row < 0 || start_row >= Terrain_FaceHeight(view->terrain)) {
        TextField_PutLine((TextField *)console, "start row out of range");
        return;
    }
    if (start_col < 0 || start_col >= Terrain_FaceWidth(view->terrain)) {
        TextField_PutLine((TextField *)console, "start col out of range");
        return;
    }
    if (end_row < 0 || end_row >= Terrain_FaceHeight(view->terrain)) {
        TextField_PutLine((TextField *)console, "end row out of range");
        return;
    }
    if (end_col < 0 || end_col >= Terrain_FaceWidth(view->terrain)) {
        TextField_PutLine((TextField *)console, "end col out of range");
        return;
    }

    const Material *material = ParseMaterial(argv[4]);
    if (material == NULL) {
        TextField_PutLine((TextField *)console, "unrecognized material");
        return;
    }

    for (int row = start_row; row <= end_row; ++row) {
        for (int col = start_col; col <= end_col; ++col) {
            Terrain_GetFace(view->terrain, row, col)->material = material;
        }
    }

    TerrainView_UpdateFaceColors(view);
}

DECLARE_RUNNABLE(terrain_raise_face, "raise-face",
    "raise (or lower) the face at (<row>, <col>) by <delta>")
{
    if (argc != 3) {
        TextField_PutLine((TextField *)console,
            "command 'terrain raise' takes three arguments");
        return;
    }

    int row = atoi(argv[0]);
    int col = atoi(argv[1]);
    if (row < 0 || row >= Terrain_FaceHeight(view->terrain)) {
        TextField_PutLine((TextField *)console, "row out of range");
        return;
    }
    if (col < 0 || col >= Terrain_FaceHeight(view->terrain)) {
        TextField_PutLine((TextField *)console, "col out of range");
        return;
    }

    int delta = atoi(argv[2]);
    Terrain_RaiseFace(view->terrain, row, col, delta);
    TerrainView_UpdateFaceHeights(view);
}

DECLARE_RUNNABLE(terrain_bulk_raise_face, "bulk-raise-face",
    "raise (or lower) the faces in a rectangular region")
{
    if (argc != 5) {
        TextField_PutLine((TextField *)console,
            "command 'terrain bulk-raise-face' takes five arguments");
        return;
    }

    int start_row = atoi(argv[0]);
    int start_col = atoi(argv[1]);
    int end_row   = atoi(argv[2]);
    int end_col   = atoi(argv[3]);

    int delta = atoi(argv[4]);

    if (start_row < 0 || start_row >= Terrain_FaceHeight(view->terrain)) {
        TextField_PutLine((TextField *)console, "start row out of range");
        return;
    }
    if (start_col < 0 || start_col >= Terrain_FaceWidth(view->terrain)) {
        TextField_PutLine((TextField *)console, "start col out of range");
        return;
    }
    if (end_row < 0 || end_row >= Terrain_FaceHeight(view->terrain)) {
        TextField_PutLine((TextField *)console, "end row out of range");
        return;
    }
    if (end_col < 0 || end_col >= Terrain_FaceWidth(view->terrain)) {
        TextField_PutLine((TextField *)console, "end col out of range");
        return;
    }

    for (int row = start_row; row <= end_row; ++row) {
        for (int col = start_col; col <= end_col; ++col) {
            Terrain_RaiseFace(view->terrain, row, col, delta);
        }
    }

    TerrainView_UpdateFaceHeights(view);
}

DECLARE_RUNNABLE(terrain_raise_vertex, "raise-vertex",
    "raise (or lower) the vertex at (<row>, <col>) by <delta>")
{
    if (argc != 3) {
        TextField_PutLine((TextField *)console,
            "command 'terrain raise' takes three arguments");
        return;
    }

    int row = atoi(argv[0]);
    int col = atoi(argv[1]);
    if (row < 0 || row >= Terrain_VertexHeight(view->terrain)) {
        TextField_PutLine((TextField *)console, "row out of range");
        return;
    }
    if (col < 0 || col >= Terrain_VertexHeight(view->terrain)) {
        TextField_PutLine((TextField *)console, "col out of range");
        return;
    }

    int delta = atoi(argv[2]);
    Terrain_RaiseVertex(view->terrain, row, col, delta);
    TerrainView_UpdateFaceHeights(view);
}

DECLARE_RUNNABLE(terrain_bulk_raise_vertex, "bulk-raise-vertex",
    "raise (or lower) the vertices in a rectangular region")
{
    if (argc != 5) {
        TextField_PutLine((TextField *)console,
            "command 'terrain bulk-raise-vertex' takes five arguments");
        return;
    }

    int start_row = atoi(argv[0]);
    int start_col = atoi(argv[1]);
    int end_row   = atoi(argv[2]);
    int end_col   = atoi(argv[3]);

    int delta = atoi(argv[4]);

    if (start_row < 0 || start_row >= Terrain_VertexHeight(view->terrain)) {
        TextField_PutLine((TextField *)console, "start row out of range");
        return;
    }
    if (start_col < 0 || start_col >= Terrain_VertexWidth(view->terrain)) {
        TextField_PutLine((TextField *)console, "start col out of range");
        return;
    }
    if (end_row < 0 || end_row >= Terrain_VertexHeight(view->terrain)) {
        TextField_PutLine((TextField *)console, "end row out of range");
        return;
    }
    if (end_col < 0 || end_col >= Terrain_VertexWidth(view->terrain)) {
        TextField_PutLine((TextField *)console, "end col out of range");
        return;
    }

    for (int row = start_row; row <= end_row; ++row) {
        for (int col = start_col; col <= end_col; ++col) {
            Terrain_RaiseVertex(view->terrain, row, col, delta);
        }
    }

    TerrainView_UpdateFaceHeights(view);
}

DECLARE_RUNNABLE(terrain_define_hole, "define-hole",
    "enter shot-points for a hole")
{
    if (argc < 5 || argc > 9) {
        // We're supposed to have a row and a column per `par - 1` (4 - 8) plus
        // one argument for the hole number.
        TextField_PutLine((TextField *)console,
            "command 'terrain define-hole' takes between 5 and 9 arguments");
        return;
    }

    int hole = atoi(argv[0]);
    if (hole < 1 || hole > 18) {
        TextField_PutLine((TextField *)console, "hole must be between 1 and 18");
        return;
    }

    Par par = (argc - 1)/2 + 1;
    ASSERT(3 <= par && par <= 5);

    uint16_t shot_points[4][2];
    for (uint8_t i = 0; i < par - 1; ++i) {
        ASSERT(1 + 2*i + 1 < argc);
        int row = atoi(argv[1 + 2*i]);
        int col = atoi(argv[1 + 2*i + 1]);

        if (row < 0 || row >= Terrain_FaceHeight(view->terrain)) {
            TextField_Printf((TextField *)console, "row %d out of range\n", i);
            return;
        }
        if (col < 0 || col >= Terrain_FaceWidth(view->terrain)) {
            TextField_Printf((TextField *)console, "col %d out of range\n", i);
            return;
        }

        shot_points[i][0] = row;
        shot_points[i][1] = col;
    }

    Terrain_DefineHole(view->terrain, hole - 1, par, shot_points);
    TerrainView_UpdateHoleLines(view);
}

DECLARE_RUNNABLE(terrain_info_normal, "normal",
    "print the normal vector for the vertex at (<row>, <col>)")
{
    if (argc != 2) {
        TextField_PutLine((TextField *)console,
            "command 'terrain info normal' takes two arguments");
        return;
    }

    int row = atoi(argv[0]);
    int col = atoi(argv[1]);
    if (row < 0 || row >= Terrain_VertexHeight(view->terrain)) {
        TextField_PutLine((TextField *)console, "row out of range");
        return;
    }
    if (col < 0 || col >= Terrain_VertexWidth(view->terrain)) {
        TextField_PutLine((TextField *)console, "col out of range");
        return;
    }

    vec3 n;
    TerrainView_VertexNormal(view, row, col, &n);
    TextField_Printf((TextField *)console, "%.3f %.3f %.3f\n", n.x, n.y, n.z);
}

DECLARE_RUNNABLE(terrain_info_height, "height",
    "print the height of the vertex at (<row>, <col>)")
{
    if (argc != 2) {
        TextField_PutLine((TextField *)console,
            "command 'terrain info normal' takes two arguments");
        return;
    }

    int row = atoi(argv[0]);
    int col = atoi(argv[1]);
    if (row < 0 || row >= Terrain_VertexHeight(view->terrain)) {
        TextField_PutLine((TextField *)console, "row out of range");
        return;
    }
    if (col < 0 || col >= Terrain_VertexWidth(view->terrain)) {
        TextField_PutLine((TextField *)console, "col out of range");
        return;
    }

    unsigned height;
    if (row == 0) {
        if (col == 0) {
            height = Terrain_GetConstFace(
                view->terrain, 0, 0)->vertices[BOTTOM_LEFT];
        } else {
            height = Terrain_GetConstFace(
                view->terrain, 0, col - 1)->vertices[BOTTOM_RIGHT];
        }
    } else {
        if (col == 0) {
            height = Terrain_GetConstFace(
                view->terrain, row - 1, 0)->vertices[TOP_LEFT];
        } else {
            height = Terrain_GetConstFace(
                view->terrain, row - 1, col - 1)->vertices[TOP_RIGHT];
        }
    }

    TextField_Printf((TextField *)console, "%u\n", height);
}

DECLARE_RUNNABLE(terrain_info_routing, "routing",
    "print information about each hole")
{
    (void)argc;
    (void)argv;

    TextField_PutLine((TextField *)console, " Hole | Par | Length");
    TextField_PutLine((TextField *)console, "------|-----|--------");
    for (uint8_t i = 0; i < 18; ++i) {
        const Hole *hole = Terrain_GetConstHole(view->terrain, i);
        TextField_Printf((TextField *)console, " %3d  |", i + 1);
        if (hole != NULL) {
            TextField_Printf((TextField *)console, "  %d  | %d\n",
                hole->par, Terrain_GetHoleLength(view->terrain, hole));
        } else {
            TextField_PutLine((TextField *)console, "  -  |   -");
        }
    }
}

DECLARE_SUB_COMMANDS(terrain_info, "info",
    "get information about various aspects of the terrain",
    &terrain_info_normal, &terrain_info_height, &terrain_info_routing);

DECLARE_SUB_COMMANDS(terrain, "terrain", "inspect and manipulate the terrain",
    &terrain_set, &terrain_bulk_set,
    &terrain_raise_face, &terrain_bulk_raise_face,
    &terrain_raise_vertex, &terrain_bulk_raise_vertex,
    &terrain_define_hole, &terrain_info);

////////////////////////////////////////////////////////////////////////////////
// HUD
//

DECLARE_RUNNABLE(hud_info, "info",
    "get information about the state of the HUD menu")
{
    (void)argv;
    (void)argc;

    switch (view->hud.selection) {
        case HUD_RAISE_FACE:
            TextField_PutLine((TextField *)console, "Selection: raise-face");
            break;
        case HUD_RAISE_VERTEX:
            TextField_PutLine((TextField *)console, "Selection: raise-vertex");
            break;
        case HUD_SET_MATERIAL:
            TextField_PutLine((TextField *)console, "Selection: set");
            TextField_Printf((TextField *)console, "Data: %s\n",
                view->hud.data.material->name);
            break;
        case HUD_NONE:
            TextField_PutLine((TextField *)console, "Selection: none");
            break;
        default:
            ASSERT(false);
    }
}

DECLARE_RUNNABLE(hud_select, "select", "select a tool from the HUD menu")
{
    if (argc == 0) {
        view->hud.selection = HUD_NONE;
        return;
    }

    if (strcmp("raise-face", argv[0]) == 0) {
        view->hud.selection = HUD_RAISE_FACE;
    } else if (strcmp("raise-vertex", argv[0]) == 0) {
        view->hud.selection = HUD_RAISE_VERTEX;
    } else if (strcmp("set", argv[0]) == 0) {
        if (argc < 2) {
            TextField_PutLine((TextField *)console,
                "need to specify name of material to set");
            return;
        }
        const Material *material = ParseMaterial(argv[1]);
        if (material == NULL) {
            TextField_PutLine((TextField *)console, "unrecognized material");
            return;
        }

        view->hud.selection = HUD_SET_MATERIAL;
        view->hud.data.material = material;
    } else {
        TextField_PutLine((TextField *)console, "unrecognized tool");
    }
}

DECLARE_SUB_COMMANDS(hud, "hud", "inspect and modify the state of the HUD menu",
    &hud_info, &hud_select);

DECLARE_PROGRAM(&show, &hide, &window, &camera, &terrain, &hud);

#undef PROGRAM_INFO
