/**
 * \file terrain_view.h
 *
 * View API for rendering `Terrain` objects with GL.
 */

#ifndef GOLF_TERRAIN_VIEW_H
#define GOLF_TERRAIN_VIEW_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "view.h"

typedef struct TerrainView TerrainView;

/**
 * \brief Allocate and initialize a terrain view.
 */
TerrainView *TerrainView_New(ViewManager *manager, Terrain *terrain);

#endif
