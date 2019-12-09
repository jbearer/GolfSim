#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GL/glew.h> // Important to include glew before other GL stuff
#include <GLFW/glfw3.h>

#include "errors.h"
#include "terrain.h"
#include "view.h"

static const int POLL_KEYS[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    ' ', '\n',
    GLFW_KEY_BACKSPACE, GLFW_KEY_DELETE,
    GLFW_KEY_RIGHT, GLFW_KEY_LEFT, GLFW_KEY_UP, GLFW_KEY_DOWN,
};

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

typedef struct {
    bool windowed;
} GolfArgs;

static void Golf_ParseArgs(int argc, char * const *argv, GolfArgs *args)
{
    static const char *short_usage =
        "golf - play golf or something\n"
        "\n"
        "Usage: golf [options]\n";

    static const char *long_usage =
        "Options\n"
        "\n"
        "  -w, --windowed\n"
        "       Launch in windowed (not fullscreen) mode\n"
        "  -h, --help\n"
        "       Show this help and exit\n";

    static const struct option long_options[] = {
        { "windowed", no_argument, 0, 'w' },
        { "help",     no_argument, 0, 'h' },
        { 0, 0, 0, 0 }
    };

    // Initialize arguments to falsey values. Arguments for which this is not a
    // sensible default should be explicitly initialized below.
    memset(args, 0, sizeof(*args));

    int option_index = 0;
    char c;
    while (
        (c = getopt_long(argc, argv, "wh", long_options, &option_index))!= -1) {

        switch (c) {
            case 'w':
                args->windowed = true;
                break;
            case 'h':
                fputs(short_usage, stdout);
                fputc('\n', stdout);
                fputs(long_usage, stdout);
                exit(0);
            case '?':
            default:
                fputs(short_usage, stderr);
                fputc('\n', stderr);
                fputs(long_usage, stderr);
                exit(1);
        }
    }
}

int main(int argc, char *const *argv)
{
    GolfArgs args;
    Golf_ParseArgs(argc, argv, &args);

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

    GLFWmonitor *monitor =
        args.windowed ? NULL
                      : glfwGetPrimaryMonitor();
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
    Terrain_Init(&terrain, 50, 50);
    View *view = View_New(window, &terrain);

    // Enable depth testing.
    glEnable(GL_DEPTH_TEST);

    // Enable color blending based on alpha channel.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // When blending a new color (the destination color) with a color
        // already in the color buffer (the source color) take the source color
        // with intensity given by the source alpha channel, and take the
        // destination color with the remaining intensity (1 - source alpha).

    // Main loop
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
