#include "errors.h"
#include "terrain.h"

const Material fairway = {
    .name = "fairway",
    .color = { 0.35, 0.6, 0.2, 1.0 },
};
const Material green = {
    .name = "green",
    .color = { 0.2, 0.9, 0.25, 1.0 },
};
const Material tee = {
    .name = "tee",
    .color = { 0.2, 0.4, 0.15, 1.0 },
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

    for (uint8_t hole = 0; hole < 18; ++hole) {
        terrain->holes[hole].par = PAR_NONE;
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

// Raise a vertex as part of a "raise face" operation, implementing the tricky
// parts of the "whole-face" semantics.
//
// `min` and `max` should be, respectively, the minimum and maximum heights of
// the four vertices adjacent to the face at (row, col). `vertex` should be the
// index of the vertex to raise (e.g. TOP_LEFT, BOTTOM_RIGHT, etc.).
static void Terrain_RaiseFaceVertex(
    Terrain *terrain,
    uint16_t min, uint16_t max,
    uint16_t row, uint16_t col, uint8_t vertex,
    int16_t delta)
{
    ASSERT(min <= max);
    ASSERT(row < Terrain_FaceHeight(terrain));
    ASSERT(col < Terrain_FaceWidth(terrain));
    ASSERT(vertex < 4);

    Face *face = Terrain_GetFace(terrain, row, col);
    uint16_t z = face->vertices[vertex];
    ASSERT(min <= z && z <= max);

    int16_t real_delta;
    if (delta > 0) {
        real_delta = IntMin(delta, max - z);
            // We know for sure we will raise the vertex at least until it is
            // level with the highest vertex.
        real_delta += IntMax((delta - real_delta) - (z - min), 0);
            // In addition, we will raise the vertex the remaining delta, except
            // that we wait for the lowest vertex to "catch up", hence the
            // difference of `z - min`.
    } else {
        real_delta = IntMax(delta, min - z);
            // We know for sure we will lower the vertex at least until it is
            // level with the lowest vertex.
        real_delta += IntMin((delta - real_delta) - (z - max), 0);
            // In addition, we will lower the vertex the remaining delta, except
            // that we wait for the highest vertex to "catch up".
    }


    // (row, col) indexes the bottom left vertex of the face, so we may need to
    // increment row and/or col to get to vertices on the top and/or right.
    if (vertex == TOP_LEFT || vertex == TOP_RIGHT) {
        row += 1;
    }
    if (vertex == TOP_RIGHT || vertex == BOTTOM_RIGHT) {
        col += 1;
    }

    Terrain_RaiseVertex(terrain, row, col, real_delta);
}

void Terrain_RaiseFace(
    Terrain *terrain, uint16_t row, uint16_t col, int16_t delta)
{
    Face *face = Terrain_GetFace(terrain, row, col);

    uint16_t min = UintMin(face->vertices[TOP_LEFT],
                   UintMin(face->vertices[TOP_RIGHT],
                   UintMin(face->vertices[BOTTOM_RIGHT],
                           face->vertices[BOTTOM_LEFT])));

    uint16_t max = UintMax(face->vertices[TOP_LEFT],
                   UintMax(face->vertices[TOP_RIGHT],
                   UintMax(face->vertices[BOTTOM_RIGHT],
                           face->vertices[BOTTOM_LEFT])));

    Terrain_RaiseFaceVertex(terrain, min, max, row, col, TOP_LEFT,     delta);
    Terrain_RaiseFaceVertex(terrain, min, max, row, col, TOP_RIGHT,    delta);
    Terrain_RaiseFaceVertex(terrain, min, max, row, col, BOTTOM_RIGHT, delta);
    Terrain_RaiseFaceVertex(terrain, min, max, row, col, BOTTOM_LEFT,  delta);
}

static float Terrain_GetVertex(
    const Terrain *terrain, uint16_t row, uint16_t col)
{
    ASSERT(row < Terrain_VertexHeight(terrain));
    ASSERT(col < Terrain_VertexWidth(terrain));

    if (row < Terrain_FaceHeight(terrain)) {
        if (col < Terrain_FaceWidth(terrain)) {
            return Terrain_GetConstFace(terrain, row, col)->vertices[BOTTOM_LEFT];
        } else {
            return Terrain_GetConstFace(terrain, row, col - 1)->vertices[BOTTOM_RIGHT];
        }
    } else {
        if (col < Terrain_FaceWidth(terrain)) {
            return Terrain_GetConstFace(terrain, row - 1, col)->vertices[TOP_LEFT];
        } else {
            return Terrain_GetConstFace(terrain, row - 1, col - 1)->vertices[TOP_RIGHT];
        }
    }
}

float Terrain_SampleHeight(const Terrain *terrain, float x, float y)
{
    ASSERT(0 <= x && x < Terrain_FaceWidth(terrain)*terrain->xy_resolution);
    ASSERT(0 <= y && y < Terrain_FaceHeight(terrain)*terrain->xy_resolution);

    // Get the coordinates relative to the face containing the point, so we can
    // figure out which triangle we will interpolate within.
    float row, col;
    float u = modff(x/terrain->xy_resolution, &row);
    float v = modff(y/terrain->xy_resolution, &col);

    // We can think of the face as being partitioned into two triangles by the
    // line `u = v`:
    //
    //   v
    //    ^        col   col+1
    //    |         |      |
    //    | row+1 --+------+--
    //    |         |    / |
    //    |         |   /  |
    //    |         |  /u=v|
    //    |         | /    |
    //    | row   --+------+--
    //    |         |      |
    //    |
    //    ----------------------> u
    //
    // We will pick the one containing (u, v) to interpolate within.
    vec2 a, b, c;
    if (v > u) {
        // Top left triangle
        a = (vec2){ col,     row + 1 };
        b = (vec2){ col,     row     };
        c = (vec2){ col + 1, row + 1 };
    } else {
        // Bottom right triangle
        a = (vec2){ col + 1, row     };
        b = (vec2){ col + 1, row + 1 };
        c = (vec2){ col,     row     };
    }

    // Now do the actual interpolation. The result will be a weighted average of
    // the z-coordinate of each vertex in the selected triangle:
    //                      Z = WaZa + WbZb + WcZc
    // The weights are chosen using barycentric coordinates, which is the
    // standard solution for interpolation within a triangle.
    //
    // A good introduction to barycentric coordinates can be found here:
    //      https://codeplea.com/triangular-interpolation
    // The bottom line is we get formulae for the weights in the above equation:
    //
    //           (b.y - c.y)(u   - c.x) + (c.x - b.x)(v   - c.y)
    //     Wa = -------------------------------------------------
    //           (b.y - c.y)(a.x - c.x) + (c.x - b.x)(a.y - c.y)
    //
    //           (c.y - a.y)(u   - c.x) + (a.x - c.x)(v   - c.y)
    //     Wb = -------------------------------------------------
    //           (b.y - c.y)(a.x - c.x) + (c.x - b.x)(a.y - c.y)
    //
    //     Wc = 1 - Wa - Wb
    //
    float Wa = ((b.y - c.y)*(u   - c.x) + (c.x - b.x)*(v   - c.y)) /
               ((b.y - c.y)*(a.x - c.x) + (c.x - b.x)*(a.y - c.y));
    float Wb = ((c.y - a.y)*(u   - c.x) + (a.x - c.x)*(v   - c.y)) /
               ((b.y - c.y)*(a.x - c.x) + (c.x - b.x)*(a.y - c.y));
    float Wc = 1 - Wa - Wb;

    return Wa*Terrain_GetVertex(terrain, round(a.y), round(a.x)) +
           Wb*Terrain_GetVertex(terrain, round(b.y), round(b.x)) +
           Wc*Terrain_GetVertex(terrain, round(c.y), round(c.x));
}

void Terrain_DefineHole(
    Terrain *terrain, uint8_t hole, Par par, uint16_t(*shot_points)[2])
{
    ASSERT(hole < 18);
    ASSERT(3 <= par && par <= 5);
    terrain->holes[hole].par = par;
    memcpy(
        terrain->holes[hole].shot_points,
        shot_points,
        (par - 1)*sizeof(shot_points[2])
    );
}

uint32_t Terrain_GetHoleLength(const Terrain *terrain, const Hole *hole)
{
    ASSERT(3 <= hole->par && hole->par <= 5);

    float length = 0;
    for (uint8_t i = 0; i < hole->par - 2; ++i) {
        // Add the length of the `i`th shot.
        vec2 shot = { (float)hole->shot_points[i + 1][1] -
                      (float)hole->shot_points[i][1]
                    , (float)hole->shot_points[i + 1][0] -
                      (float)hole->shot_points[i][0]
                    };
        length += vec2_Norm(&shot);
    }

    return round(length*terrain->xy_resolution);
        // The length is now in row/col units. We have to multiply by the
        // horizontal resolution to get it in units of yards.
}
