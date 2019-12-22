/**
 * \file view.h
 * \brief Virtualization of the global window.
 *
 * A `View` is an abstraction similar to a `GLFWwindow`. The primary difference
 * is that access to the `GLFWwindow` is global, while multiple `View` objects
 * can share a window simultaneously. What this means is that each `View` can
 * have its own handlers for mouse and keyboard input, and its own per-view
 * state, and the user's input to the window will be multiplexed across all the
 * `View` objects.
 *
 * In addition to the primary task of multiplexing the window across a handful
 * of views, the `View` abstraction is also a convenient way to throw in a
 * couple of nice, extra features:
 *  * Views can have subviews. In fact, the structure of the relationships
 *    between views can get quite complex if needed. It forms a forest in
 *    general. See below for details.
 *  * Each view can optionally attach a `Console` program. The view manager will
 *    ensure that the console program for the focused view runs whenever the
 *    user enters the shorcut Ctrl+Shift+P. The architectural decision to have
 *    the view manager handle the consoles is beneficial for debugging, because
 *    it makes it likely that even if a particular component is buggy, the
 *    console attached to that component will still work.
 *  * Views often want to create animations whose rate is independent of the
 *    frame rate. The view manager handles the task of recording the time at
 *    which each frame is rendered and passing a time delta to the render method
 *    of each view.
 *  * GLFW is a fairly hefty dependency, and we don't use a lot of it. Having
 *    this custom interface means that, should we ever want to scrap GLFW and
 *    replace it with something lighter weight, we will be able to do so mostly
 *    transparently.
 *
 * As a practical example of why this is abstraction is necessary: two views
 * which frequently share a window at the same time are the \ref terrain_view.h
 * "TerrainView" and the `Console`. The `TerrainView` installs a mouse input
 * handler, and the `Console` installs a keyboard input handler. Both of these
 * handlers need some state to do their jobs, but with GLFW callbacks, the only
 * state we have access to is the one, global window user pointer. The `View`
 * abstraction allows both of these views to share the window, and acces their
 * own local state in their respective handlers.
 *
 * ## The `ViewManager`
 *
 * This sort of multiplexing requires us to choose which `View` should handle
 * each window input event. The way we make these decisions is with a familiar
 * concept in window abstractions: focus.
 *
 * Each window has a global pointer to the focused `View`. This global pointer
 * is managed by the `ViewManager`, which is the structure responsible for
 * maintaining focus, dispatching events from the window, managing memory for
 * view, and generally providing an interface to the main loop of the
 * application.
 *
 * When the `ViewManager` receives an event from the window, it tries to handle
 * it by dispatching it to the focused view. If the focused view has not set up
 * a handler for that event, the manager will try dispatching the event to the
 * focused view's parent, and so on until it reaches a top-level view. If the
 * top-level view cannot handle the event, the event will be dropped.
 *
 * Whenever the window is rendered, the `ViewManager` will find the root view of
 * the focused view, by repeatedly following parent pointers until reaching a
 * top-level view. It will then render the entire root tree containing the
 * focused view, starting at the root and working breadth-first towards the
 * leaves.
 *
 * ## Relationships between views
 *
 * As alluded to above, all of the views managed by a `ViewManager` form a
 * forest. Since we also maintain various back-pointers between views, the
 * relationships can get somewhat complicated. Each `View` has four important
 * pointers to other views:
 *  * `parent`: a pointer to the parent of this view, or NULL if this is a top-
 *    level view.
 *  * `children`: a pointer to the first node in a list of sub-views of this
 *    view.
 *  * `next_sibling`: a pointer to the next view in the list of siblings of this
 *    view, or, equivalently, of children of this view's parent.
 *  * `prev_sibling`: a pointer to the previous view in the sibling list.
 * In addition, the `ViewManager` itself has two important pointers:
 *  * `roots`: a pointer to the first view in a list of top-level views, linked
 *    by `next_sibling` and `next_prev`.
 *  * `focused`: a pointer to the focused view.
 *
 * At this point an example seems useful, so here is an illustration of the
 * relationships between views in a scenario where:
 *  * We have two top-level views, a `TerrainView` and a hypothetical menu.
 *  * The terrain view has one child, a `Console`, which is focused.
 *  * The `MenuView` has two children, a `Console` and a `ButtonView`, which in
 *    turn has a `TextField` child.
 *
 *
 *             ViewManager--------+
 *               |                |
 *               |                |roots
 *               |                |         next_sibling
 *               |                V      ----------------->
 *               |            TerrainView                  MenuView
 *       focused |               |   ^   <-----------------| ^    ^
 *               |               |   |      prev_sibling   | |    |
 *               |               |   |                     | |    |
 *               |      children |   | parent     children | |    | parent
 *               |               V   |                     V |    |
 *               |             ---------            ButtonView--->Console
 *               +-----------> |Console|                 |    <---
 *                             ---------                 |
 *                                                       V
 *                                                   TextField
 *
 * Because of these complicated relationships between views, it is important
 * that the application only interact with views through the `ViewManager`
 * interface, which internally manages all of these pointers. In particular,
 * regarding memory management, views should only be allocated with `View_New`
 * (or a view-specific function which calls `View_New`, such as
 * `TerrainView_New`) and views will automatically be freed when the view is
 * closed or when the manager is destroyed.
 */

#ifndef GOLF_VIEW_H
#define GOLF_VIEW_H

#include <stdbool.h>

#include <GLFW/glfw3.h>

/**
 * \defgroup ViewManager ViewManager: Interface to the main application.
 * @{
 */

typedef struct ViewManager {
    GLFWwindow *window;

    struct View *roots;
        // First view in the list of top-level trees.
    struct View *focused;
        // Focused view.
    uint32_t last_time;
        // Absolute time in milliseconds when the last frame was rendered (or
        // when the manager was created, if nothing has been rendered yet).
} ViewManager;

/**
 * \brief Initialize a view manager to manager the given window.
 *
 * This function takes full ownership of `window`. After `ViewManager_Init` is
 * called, and before a corresponding `ViewManager_Destroy` returns, GLFW
 * functions should not be used to interact with `window`. Only use
 * `ViewManager` functions.
 */
void ViewManager_Init(ViewManager *manager, GLFWwindow *window);

/**
 * \brief Close a `ViewManager`.
 *
 * Closes and destroys all views managed by `manager` as if by calling
 * `View_Close`. Causes `manager` to relinquish control of the managed window.
 * After this function returns, the window may once again be used as a normal
 * GLFW window.
 *
 * After `ViewManager_Destroy` returns, `manager` is in an invalid state, and
 * should not be used again, unless reinitialized by another call to
 * `ViewManager_Init`.
 */
void ViewManager_Destroy(ViewManager *manager);

/**
 * \brief Draw the focused view and its related views to the window.
 */
void ViewManager_Render(ViewManager *manager);

/**
 * @}
 *
 * \defgroup Input Asynchronous Input
 * @{
 */

typedef enum {
    KEY_PRESS = GLFW_PRESS,
    KEY_RELEASE = GLFW_RELEASE,
    KEY_REPEAT = GLFW_REPEAT,
} KeyAction;

typedef enum {
    MOD_SHIFT = GLFW_MOD_SHIFT,
    MOD_CONTROL = GLFW_MOD_CONTROL,
} ModifierKey;

typedef enum {
    MOUSE_PRESS = GLFW_PRESS,
    MOUSE_DRAG,
    MOUSE_RELEASE = GLFW_RELEASE,
} MouseAction;

typedef enum {
    MOUSE_BUTTON_LEFT = GLFW_MOUSE_BUTTON_LEFT,
    MOUSE_BUTTON_RIGHT = GLFW_MOUSE_BUTTON_RIGHT,
    MOUSE_BUTTON_MIDDLE = GLFW_MOUSE_BUTTON_MIDDLE,
} MouseButton;

/**
 * \brief Callback used to render a view.
 *
 * \param view  The `View` which called `View_SetRenderCallback`.
 * \param dt    Time in milliseconds since the last frame was rendered.
 */
typedef void (*View_RenderFunction)(struct View *view, uint32_t dt);

/**
 * \brief Callback used to destroy a view when it is closed.
 *
 * These functions should release all resources which were successfully acquired
 * in the views constructor (whichever function called `View_New`).
 */
typedef void (*View_DestroyFunction)(struct View *);

/**
 * \brief Callback used to dispatch a keyboard event to a view.
 *
 * \param key    A GLFW keycode (e.g. GLFW_KEY_A).
 * \param action The action that triggered the event.
 * \param mods   Modifier keys which were held down when the event triggered.
 *               This will be a bitwise or of zero or more `ModifierKey` values.
 */
typedef void (*View_KeyFunction)(
    struct View *, int key, KeyAction action, ModifierKey mods);

/**
 * \brief Callback used to dispatch a character event to a view.
 *
 * \param codepoint A UTF-32 codepoint.
 *
 * Character events are similar to keyboard events -- in fact they are usually
 * triggered by keyboard events. However, character events go through an extra
 * pass of processing in the operating system before being dispatched to the
 * view. The data that is ultimately passed to the callback is the unicode
 * codepoint generated by the combination of keys and modifier keys pressed,
 * given the users keymap, locale, etc.
 *
 */
typedef void (*View_CharacterFunction)(struct View *, uint32_t codepoint);

/**
 * \brief Callback used to dispatch a mouse event to a view.
 *
 * \param button The mouse button that triggered the event.
 * \param action The action that triggered the event.
 * \param mods   Modifier keys which were held down when the event triggered.
 *               This will be a bitwise or of zero or more `ModifierKey` values.
 */
typedef void (*View_MouseButtonFunction)(
    struct View *, MouseButton button, MouseAction action, ModifierKey mods);

/**
 * \brief Callback used to dispatch 2-dimensional scroll events to a view.
 *
 * \param x     The distance to scroll in the `x` direction.
 * \param y     The distance to scroll in the `y` direction.
 *
 * \note
 *  The units of `x` and `y` are a little arbitrary. They correspond to "number
 *  of clicks of the scroll wheel", and will almost always be -1, 0, or 1.
 */
typedef void (*View_ScrollFunction)(struct View *, int32_t x, int32_t y);

/// @}

struct Command;

/**
 * \defgroup View View: Interface to the view itself, and other views.
 * @{
 */

typedef struct View {
    // Parents
    struct ViewManager *manager;
    struct View *parent;

    // Siblings
    struct View *next_sibling;
    struct View *prev_sibling;

    // Children
    struct View *children;

    // Traversals
    struct View *queue_next;
        // This pointer is only used as a temporary in functions that traverse a
        // view tree. It is used by those functions to build a queue of Views in
        // order to perform a breadth-first traversal. Outside of those specific
        // functions, this pointer should always be NULL.

    // Console
    struct Console *console;
        // Each view has an optional console for debugging.
    const struct Command *console_program;
    void *console_state;
        // The Console view itself comes and goes, as we close it when it loses
        // focus. However, when the user enters the console shortcut, we need to
        // recreate the console again, so we save the program that it was
        // running and the state.

    // Callbacks
    View_RenderFunction render;
    View_DestroyFunction destroy;
    View_KeyFunction key_callback;
    View_CharacterFunction character_callback;
    View_MouseButtonFunction mouse_button_callback;
    View_ScrollFunction scroll_callback;
} View;

/**
 * \brief Allocate and initialize a new `View`.
 *
 * \param size      The size of the object to allocate. This allows this
 *                  function to be used to initialize the `View` base class
 *                  portion of a derived class `T`, in which case `size` should
 *                  be `sizeof(T)`.
 * \param manager   The `ViewManager` for the window which will contain this
 *                  view.
 * \param parent    If non-NULL, the new view will be added as a child of
 *                  `parent`. Otherwise, the new view becomes a top-level view.
 *
 * The new view will not be focused until explicitly given the focus, for
 * example via `View_Focus`.
 *
 * \pre `size >= sizeof(View)`.
 */
View *View_New(size_t size, struct ViewManager *manager, View *parent);

/**
 * \brief Set a console program to use when the view is focused.
 */
void View_UseProgram(View *view, const struct Command *program, void *state);

/**
 * \brief Set a callback to run when the view gets a key event.
 *
 * \return The old callback, if there was one, or `NULL`.
 */
static inline View_KeyFunction View_SetKeyCallback(
    View *view, View_KeyFunction fn)
{
    View_KeyFunction old_fn = view->key_callback;
    view->key_callback = fn;
    return old_fn;
}

/**
 * \brief Set a callback to run when the view gets a character event.
 *
 * \return The old callback, if there was one, or `NULL`.
 */
static inline View_CharacterFunction View_SetCharacterCallback(
    View *view, View_CharacterFunction fn)
{
    View_CharacterFunction old_fn = view->character_callback;
    view->character_callback = fn;
    return old_fn;
}

/**
 * \brief Set a callback to run when the view gets a mouse event.
 *
 * \return The old callback, if there was one, or `NULL`.
 */
static inline View_MouseButtonFunction View_SetMouseButtonCallback(
    View *view, View_MouseButtonFunction fn)
{
    View_MouseButtonFunction old_fn = view->mouse_button_callback;
    view->mouse_button_callback = fn;
    return old_fn;
}

/**
 * \brief Set a callback to run when the view gets a scroll event.
 *
 * \return The old callback, if there was one, or `NULL`.
 */
static inline View_ScrollFunction View_SetScrollCallback(
    View *view, View_ScrollFunction fn)
{
    View_ScrollFunction old_fn = view->scroll_callback;
    view->scroll_callback = fn;
    return old_fn;
}

/**
 * \brief Set a callback to run when the view is rendered.
 *
 * \return The old callback, if there was one, or `NULL`.
 */
static inline View_RenderFunction View_SetRenderCallback(
    View *view, View_RenderFunction fn)
{
    View_RenderFunction old_fn = view->render;
    view->render = fn;
    return old_fn;
}

/**
 * \brief Set a callback to run when the view is closed.
 *
 * \return The old callback, if there was one, or `NULL`.
 */
static inline View_DestroyFunction View_SetDestroyCallback(
    View *view, View_DestroyFunction fn)
{
    View_DestroyFunction old_fn = view->destroy;
    view->destroy = fn;
    return old_fn;
}

/**
 * \brief Give focus to a view.
 */
void View_Focus(View *view);

/**
 * \brief Get the `ViewManager` which is in charge of this view.
 */
static inline ViewManager *View_GetManager(View *view)
{
    return view->manager;
}

/**
 * \brief Get the dimensions of the window containing this view.
 */
void View_GetWindowSize(const View *view, uint32_t *width, uint32_t *height);

/**
 * \brief Get the position of the cursor in the window containing this view.
 */
void View_GetCursorPos(const View *view, int32_t *x, int32_t *y);

/**
 * \brief Close and destroy the view.
 *
 * The view is removed from all of the manager's data structures. If the view
 * was focused, the focus is shifted elsewhere. The view and all of its sub-
 * views are then destroyed by calling their registered destructor callbacks
 * (see `View_SetDestroyCallback`) and then freeing their memory.
 */
void View_Close(View *view);

/// @}

#endif
