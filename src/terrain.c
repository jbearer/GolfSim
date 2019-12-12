#include "errors.h"
#include "terrain.h"

const Material fairway = {
    .color = { 0.2, 0.9, 0.25, 1.0 }
};
const Material rough = {
    .color = { 0.1, 0.25, 0.1, 1.0 }
};
const Material sand = {
    .color = { 0.8, 0.8, 0.1, 1.0 }
};
const Material water = {
    .color = { 0.1, 0.1, 0.7, 1.0 }
};

void Terrain_Init(Terrain *terrain, uint16_t width, uint16_t height)
{
    terrain->width = width;
    terrain->height = height;
    terrain->faces = Malloc(Terrain_NumFaces(terrain)*sizeof(Face));

    for (uint16_t row = 0; row < Terrain_FaceHeight(terrain); ++row) {
        for (uint16_t col = 0; col < Terrain_FaceWidth(terrain); ++col) {
            Face *face = Terrain_GetFace(terrain, row, col);
            memset(face->vertices, 0, sizeof(face->vertices));
            face->material = &rough;
        }
    }
}
