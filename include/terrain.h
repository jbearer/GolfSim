/**
 * \file terrain.h
 *
 * Internal representation of a terrain mesh.
 */

#ifndef GOLF_TERRAIN_H
#define GOLF_TERRAIN_H

#include <stdint.h>

#include "errors.h"
#include "matrix.h"

typedef struct {
    vec4 color;
    const char *name;
} Material;

extern const Material fairway;
extern const Material green;
extern const Material tee;
extern const Material rough;
extern const Material sand;
extern const Material water;

typedef struct {
    uint16_t vertices[4];       ///< \brief z-coordinate of the four vertices
                                ///<
                                ///< ```
                                ///<    vertices[0]     vertices[1]
                                ///<        o----------------o
                                ///<        |                |
                                ///<        |                |
                                ///<        |                |
                                ///<        |                |
                                ///<        |                |
                                ///<        o----------------o
                                ///<    vertices[3]     vertices[2]
                                ///< ```
                                ///<
    const Material *material;   ///< Material covering this area.
} Face;

// Indices of vertices in Face::vertices, by spatial position.
#define TOP_LEFT     0
#define TOP_RIGHT    1
#define BOTTOM_RIGHT 2
#define BOTTOM_LEFT  3

typedef struct {
    uint16_t width;
        ///< Number of edges
    uint16_t height;
        ///< Number of edges
    uint8_t xy_resolution;
        ///< Resolution in the XY plane; that is, the length or width of a face
    Face *faces;
        ///< Dimension width x height
} Terrain;

/**
 * \brief The width of the terrain in faces.
 *
 * \details
 *      This is the number of edges along the top or bottom edge of the terrain.
 */
static inline uint16_t Terrain_FaceWidth(const Terrain *terrain)
{
    return terrain->width;
}

/**
 * \brief The height of the terrain in faces.
 *
 * \details
 *      This is the number of edges along the left or right edge of the terrain.
 */
static inline uint16_t Terrain_FaceHeight(const Terrain *terrain)
{
    return terrain->height;
}

/**
 * \brief Total number of faces in the terrain.
 *
 * \details
 *      Equivalent to `Terrain_FaceWidth(terrain) * Terrain_FaceHeight(terrain)`
 */
static inline uint32_t Terrain_NumFaces(const Terrain *terrain)
{
    return Terrain_FaceWidth(terrain) * Terrain_FaceHeight(terrain);
}

/**
 * \brief The width of the terrain in vertices.
 *
 * \details
 *      This is the number of vertices along the top or bottom edge of the
 *      terrain. Equivalent to `Terrain_FaceWidth(terrain) + 1`.
 */
static inline uint16_t Terrain_VertexWidth(const Terrain *terrain)
{
    return 1 + Terrain_FaceWidth(terrain);
}

/**
 * \brief The height of the terrain in vertices.
 *
 * \details
 *      This is the number of vertices along the left or right edge of the
 *      terrain. Equivalent to `Terrain_FaceHeight(terrain) + 1`.
 */
static inline uint16_t Terrain_VertexHeight(const Terrain *terrain)
{
    return 1 + Terrain_FaceHeight(terrain);
}

/**
 * \brief Total number of vertices in the terrain.
 *
 * \details
 *      Equivalent to
 *      `Terrain_VertexWidth(terrain) * Terrain_VertexHeight(terrain)`.
 */
static inline uint32_t Terrain_NumVertices(const Terrain *terrain)
{
    return Terrain_VertexWidth(terrain) * Terrain_VertexHeight(terrain);
}

/**
 * \brief Get a reference to the face at a given position.
 *
 * \pre
 * `row < Terrain_FaceHeight(terrain)`
 *
 * \pre
 * `col < Terrain_FaceWidth(terrain)`
 */
static inline Face *Terrain_GetFace(
    Terrain *terrain, uint16_t row, uint16_t col)
{
    ASSERT(row < Terrain_FaceHeight(terrain));
    ASSERT(col < Terrain_FaceWidth(terrain));
    return &terrain->faces[row*Terrain_FaceWidth(terrain) + col];
}

/**
 * \brief Get a reference to the face at a given position.
 *
 * \pre
 * `row < Terrain_FaceHeight(terrain)`
 *
 * \pre
 * `col < Terrain_FaceWidth(terrain)`
 */
static inline const Face *Terrain_GetConstFace(
    const Terrain *terrain, uint16_t row, uint16_t col)
{
    ASSERT(row < Terrain_FaceHeight(terrain));
    ASSERT(col < Terrain_FaceWidth(terrain));
    return &terrain->faces[row*Terrain_FaceWidth(terrain) + col];
}

/**
 * \brief Initialize a terrain object.
 *
 * \param width         The width of the terrain in faces.
 * \param height        The height of the terrain in faces.
 * \param xy_resolution The length and width of each face, in yards.
 */
void Terrain_Init(Terrain *terrain,
    uint16_t width, uint16_t height, uint8_t xy_resolution);

/**
 * \brief Release resources acquired in `Terrain_Init`.
 *
 * After this function returns, the `Terrain` object pointed to by `terrain` is
 * no longer valid, and should not be used again, unless it is reinitialized by
 * another call to `Terrain_Init`.
 */
void Terrain_Destroy(Terrain *terrain);

/**
 * \brief Raise or lower the z-coordinate of a face.
 *
 * \param row   The row of the face on which to operate.
 * \param col   The column of the face on which to operate.
 * \param delta The change to apply to the z-coordinate of each vertex in the
 *              specified face. A positive delta raises the face; a negative
 *              delta lowers it.
 *
 * If the face is currently level (all four vertices have the same height) each
 * vertex in the face is raised or lowered individually as if by
 * `Terrain_RaiseVertex`. This means that the lower-bound behavior applied by
 * `Terrain_RaiseVertex` is applied to each vertex independently: if `vertex +
 * delta < 0` for any vertex, the height of that vertex will be set to 0. If
 * `vertex + delta >= 0` for any vertex, that vertex will be raised or lowered
 * the full amount, _even if_ some other vertex was truncated at 0.
 *
 *
 * ## Whole-face semantics
 *
 * If the face is not level, the extremal vertex (or vertices) is not affected
 * by the change, until the face becomes level. For example, raising by `1` will
 * have no affect on the highest vertex (or vertices). Lowering by `-1` will
 * have no affecet on the lowest vertex. If the magnitude of `delta` is greater
 * than `1`, and a smaller delta would cause the face to be level, the off-level
 * vertices will be raised by a portion of `delta` until the face is level, and
 * then all vertices will be raised by the remainder of delta.
 *
 * \pre
 * `row < Terrain_FaceHeight(terrain)`
 *
 * \pre
 * `col < Terrain_FaceWidth(terrain)`
 */
void Terrain_RaiseFace(
    Terrain *terrain, uint16_t row, uint16_t col, int16_t delta);

/**
 * \brief Raise or lower the z-coordinate of a vertex.
 *
 * \param row   The row of the vertex on which to operate.
 * \param col   The column of the vertex on which to operate.
 * \param delta The change to apply to the z-coordinate of the vertex. A
 *              positive delta raises the vertex; a negative delta lowers it.
 *
 * If `vertex + delta >= 0`, the new height of the vertex will be
 * `vertex + delta`. Otherwise, if `vertex + delta < 0`, the new height of the
 * vertex will be 0.
 *
 * This change affects all vertices that share this row and column. There may be
 * up to four such vertices: one for each face which has a corner here.
 *
 * \pre
 * `row < Terrain_VertexHeight(terrain)`
 *
 * \pre
 * `col < Terrain_VertexWidth(terrain)`
 */
void Terrain_RaiseVertex(
    Terrain *terrain, uint16_t row, uint16_t col, int16_t delta);

#endif
