#include <stdlib.h>

#include "errors.h"
#include "terrain.h"

const Material fairway = {
    .color = { 0.2, 0.9, 0.25, 1.0 }
};
const Material rough = {
    .color = { 0.1, 0.25, 0.1, 1.0 }
};

void Terrain_Init(Terrain *terrain, uint16_t width, uint16_t height)
{
    terrain->width = width;
    terrain->height = height;
    terrain->faces = calloc(Terrain_NumFaces(terrain), sizeof(Face));
    if (terrain->faces == NULL) {
        Error_Raise(FATAL, ERR_OUT_OF_MEMORY, "terrain faces");
    }
}
