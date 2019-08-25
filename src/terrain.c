#include <stdlib.h>

#include "errors.h"
#include "terrain.h"

void Terrain_Init(Terrain *terrain, uint16_t width, uint16_t height)
{
    terrain->width = width;
    terrain->height = height;
    terrain->vertices = calloc(Terrain_NumVertices(terrain), sizeof(Vertex));
    if (terrain->vertices == NULL) {
        Error_Raise(FATAL, ERR_OUT_OF_MEMORY, "terrain vertices");
    }
}
