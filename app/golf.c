#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GL/glew.h> // Important to include glew before other GL stuff
#include <GLFW/glfw3.h>

#include "errors.h"
#include "terrain.h"
#include "view.h"

static void GlError(int error, const char *msg)
{
    fprintf(stderr, "OpenGL error %d: %s\n", error, msg);
    glfwTerminate();
    exit(1);
}

static void GolfError(Error error, void *error_data, void *arg)
{
    (void)error;
    (void)error_data;
    (void)arg;
    glfwTerminate();
}

int main(void)
{
    glewExperimental = true;

    Error_SetFatalErrorCallback(GolfError, NULL);
    glfwSetErrorCallback(GlError);

    // Initialize GLFW
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        exit(1);
    }

    // Open a window
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        // Window will be fullscreen on the primary monitor.
    GLFWwindow *window = glfwCreateWindow(1024, 768, "Golf", monitor, NULL);
    if (window == NULL) {
        fprintf(stderr, "Could not open window.\n");
        goto ERR_CREATE_WINDOW;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        goto ERR_GLEW_INIT;
    }

    // Initialize game objects
    Terrain terrain;
    Terrain_Init(&terrain, 256, 256);
    View *view = View_New(window, &terrain);

    // Main loop
    glEnable(GL_DEPTH_TEST);
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        View_Render(view);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;

ERR_GLEW_INIT:
ERR_CREATE_WINDOW:
    glfwTerminate();
    return 1;
}
