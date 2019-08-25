/**
 * \file view.h
 *
 * View API for rendering `Terrain` objects with GL.
 */

#ifndef GOLF_VIEW_H
#define GOLF_VIEW_H

typedef struct View View;

/**
 * \brief Allocate and initialize a terrain view.
 */
View *View_New(Terrain *terrain);

/**
 * \brief Render a terrain view to the screen.
 */
void View_Render(View *view);

#endif
