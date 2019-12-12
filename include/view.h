/**
 * \file view.h
 *
 * View API for rendering `Terrain` objects with GL.
 */

#ifndef GOLF_VIEW_H
#define GOLF_VIEW_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>

typedef struct View View;

/**
 * \brief Allocate and initialize a terrain view.
 */
View *View_New(GLFWwindow *window, const Terrain *terrain);

/**
 * \brief Render a terrain view to the screen.
 *
 * \param view The `View` to render.
 * \param dt   The elapsed time in milliseconds since the last frame was
 *             rendered. This can be used by the implementation to create
 *             animations whose speed does not depend on the frame rate.
 */
void View_Render(View *view, uint32_t dt);

#endif
