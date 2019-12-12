/**
 * \file terrain.h
 *
 * Internal representation of a terrain mesh.
 */

#ifndef GOLF_TERRAIN_H
#define GOLF_TERRAIN_H

#include <assert.h>
#include <stdint.h>

#include "matrix.h"

typedef struct {
    vec4 color;
} Material;

extern const Material fairway;
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

typedef struct {
    uint16_t width;     ///< Number of edges
    uint16_t height;    ///< Number of edges
    Face *faces;        ///< Dimension width x height
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
 * `row < Terrain_FaceWidth(terrain)`
 *
 * \pre
 * `col < Terrain_FaceHeight(terrain)`
 */
static inline Face *Terrain_GetFace(
    Terrain *terrain, uint16_t row, uint16_t col)
{
    assert(row < Terrain_FaceHeight(terrain));
    assert(col < Terrain_FaceWidth(terrain));
    return &terrain->faces[row*Terrain_FaceWidth(terrain) + col];
}

/**
 * \brief Get a reference to the face at a given position.
 *
 * \pre
 * `row < Terrain_FaceWidth(terrain)`
 *
 * \pre
 * `col < Terrain_FaceHeight(terrain)`
 */
static inline const Face *Terrain_GetConstFace(
    const Terrain *terrain, uint16_t row, uint16_t col)
{
    assert(row < Terrain_FaceHeight(terrain));
    assert(col < Terrain_FaceWidth(terrain));
    return &terrain->faces[row*Terrain_FaceWidth(terrain) + col];
}

/**
 * \brief Initialize a terrain object.
 *
 * \param width  The width of the terrain in faces.
 * \param height The height of the terrain in faces.
 */
void Terrain_Init(Terrain *terrain, uint16_t width, uint16_t height);

#endif
