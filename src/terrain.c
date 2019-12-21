#include "errors.h"
#include "terrain.h"

const Material fairway = {
    .name = "fairway",
    .color = { 0.2, 0.9, 0.25, 1.0 },
};
const Material rough = {
    .name = "rough",
    .color = { 0.1, 0.25, 0.1, 1.0 },
};
const Material sand = {
    .name = "sand",
    .color = { 0.8, 0.8, 0.1, 1.0 },
};
const Material water = {
    .name = "water",
    .color = { 0.1, 0.1, 0.7, 1.0 },
};

void Terrain_Init(Terrain *terrain,
    uint16_t width, uint16_t height, uint8_t xy_resolution)
{
    terrain->width = width;
    terrain->height = height;
    terrain->xy_resolution = xy_resolution;
    terrain->faces = Malloc(Terrain_NumFaces(terrain)*sizeof(Face));

    for (uint16_t row = 0; row < Terrain_FaceHeight(terrain); ++row) {
        for (uint16_t col = 0; col < Terrain_FaceWidth(terrain); ++col) {
            Face *face = Terrain_GetFace(terrain, row, col);
            memset(face->vertices, 0, sizeof(face->vertices));
            face->material = &rough;
        }
    }
}

void Terrain_Destroy(Terrain *terrain)
{
    free(terrain->faces);
}

static void Face_RaiseVertex(
    Terrain *terrain, uint16_t row, uint16_t col, uint8_t v, int16_t delta)
{
    ASSERT(row < Terrain_FaceHeight(terrain));
    ASSERT(col < Terrain_FaceWidth(terrain));
    ASSERT(v < 4);

    Face *face = Terrain_GetFace(terrain, row, col);
    uint16_t *vertex = &face->vertices[v];

    if (delta < 0 && -delta > *vertex) {
        *vertex = 0;
    } else {
        *vertex += delta;
    }
}

void Terrain_RaiseVertex(
    Terrain *terrain, uint16_t row, uint16_t col, int16_t delta)
{
    ASSERT(row <= Terrain_FaceHeight(terrain));
    ASSERT(col <= Terrain_FaceWidth(terrain));

    // We have a vertex at the intersection of up to four faces:
    //
    //                   col
    //          ----------------------
    //          |         |          |
    //          |   F1    |    F2    |
    //          |       V |          |
    //     row  ----------*-----------
    //          |         |          |
    //          |   F4    |    F3    |
    //          |         |          |
    //          ----------------------
    //
    // We will raise this vertex by raising the appropriate vertex of each of
    // these faces independently.

    // F1
    if (row < Terrain_FaceHeight(terrain) && col > 0) {
        Face_RaiseVertex(terrain, row, col - 1, BOTTOM_RIGHT, delta);
    }
    // F2
    if (row < Terrain_FaceHeight(terrain) && col < Terrain_FaceWidth(terrain)) {
        Face_RaiseVertex(terrain, row, col, BOTTOM_LEFT, delta);
    }
    // F3
    if (row > 0                           && col < Terrain_FaceWidth(terrain)) {
        Face_RaiseVertex(terrain, row - 1, col, TOP_LEFT, delta);
    }
    // F4
    if (row > 0                           && col > 0) {
        Face_RaiseVertex(terrain, row - 1, col - 1, TOP_RIGHT, delta);
    }
}

void Terrain_RaiseFace(
    Terrain *terrain, uint16_t row, uint16_t col, int16_t delta)
{
    Terrain_RaiseVertex(terrain, row,   col,   delta);
    Terrain_RaiseVertex(terrain, row+1, col,   delta);
    Terrain_RaiseVertex(terrain, row+1, col+1, delta);
    Terrain_RaiseVertex(terrain, row,   col+1, delta);
}
