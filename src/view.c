#include <GLFW/glfw3.h>

#include "errors.h"
#include "clock.h"
#include "text.h"
#include "view.h"

////////////////////////////////////////////////////////////////////////////////
// Traversing views
//
// Various functions need to process each View in a view tree in breadth-first
// order. `ViewTraversal` provides a generic API for doing so.
//
// The traversal implemented here is performed in such away that a view is never
// inspected by the traversal code after it is processed. This means that the
// client doing the traversal can really do whatever they want with the views
// being processed -- even free them.

// State maintained by the traversal. To users of the traversal, this should be
// considered opaque. Internally, it stores a queue containing views which still
// need to be visited.
typedef struct {
    View *head;
    View *tail;
} ViewTraversal;

// Helper function to pop from a `ViewTraversal` queue. Not part of the public
// traversal API.
static inline View *View_TraversalPop(ViewTraversal *queue)
{
    if (queue->head == NULL) {
        return NULL;
    }

    ASSERT(queue->head != NULL);
    ASSERT(queue->tail != NULL);
    ASSERT(queue->tail->queue_next == NULL);

    View *head = queue->head;
    queue->head = head->queue_next;
    head->queue_next = NULL;
    if (queue->tail == head) {
        queue->tail = head->queue_next;
    }

    return head;
}

// Helper function to push to a `ViewTraversal` queue. Not part of the public
// traversal API.
static inline void View_TraversalPush(ViewTraversal *queue, View *view)
{
    ASSERT(view->queue_next == NULL);
    if (queue->tail == NULL) {
        queue->head = view;
        queue->tail = view;
    } else {
        ASSERT(queue->tail->queue_next == NULL);
        queue->tail->queue_next = view;
        queue->tail = view;
    }
}

// Initialize a `ViewTraversal` for the tree rooted at `view`.
static ViewTraversal View_Traversal(View *view)
{
    return (ViewTraversal) { .head = view, .tail = view };
        // Initially, the queue just contains `view`. We will add the children
        // of `view` after we pop and process `view`.
}

// Get the next `View` in a traversal, or NULL if the traversal is finished.
static View *View_Traverse(ViewTraversal *queue)
{
    View *view = View_TraversalPop(queue);
    if (view == NULL) {
        return NULL;
    }

    // Add the children of `view` to the queue so we remember they need to be
    // processed.
    for (View *child = view->children;
         child != NULL;
         child = child->next_sibling) {
        View_TraversalPush(queue, child);
    }

    return view;
        // As soon as we return `view` from this function, we are no longer
        // allowed to use it, because the caller is allowed to do anything they
        // want with it, including close it. We know this is safe because
        //  * we popped `view` off of the queue to start this function, so it
        //    isn't queued up for further processing, and
        //  * everything else on the queue is a child of `view` or a descendant
        //    of a sibling of `view`. Given the tree structure of views (no
        //    cycles) this means that `view` cannot possibly come up again in
        //    the traversal.
}

// Get the root of the tree containing `view`.
static View *View_Root(View *view)
{
    while (view->parent != NULL) {
        view = view->parent;
    }

    return view;
}

////////////////////////////////////////////////////////////////////////////////
// Window-level callbacks
//
// The `ViewManager` installs several global-per-window callbacks with GLFW.
// These callbacks are generally simple; their only job is to get an event, find
// the most focused view with a handler for that event, and dispatch the event
// to that view.
//
// The exception is `ViewManager_KeyCallback`, which does some special
// processing to handle the global "toggle console" keyboard shortcut.
//

static void ViewManager_KeyCallback(
    GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void)scancode;

    // The manager for this window is stored in the window user pointer.
    ViewManager *manager = glfwGetWindowUserPointer(window);
    ASSERT(manager != NULL);

    View *view = manager->focused;

    if (action == GLFW_PRESS &&
        key == GLFW_KEY_P &&
        (mods & GLFW_MOD_CONTROL) &&
        (mods & GLFW_MOD_SHIFT)) {

        // Ctrl+Shift+P toggles focus of the console. To handle it, we need to
        // figure out if the focused view is a console (in which case we should
        // close it) or if the current view has a console (in which case we
        // should display it).
        //
        // Since the view manager is in charge of creating and destroying
        // consoles, we can trust that all consoles will obey the following
        // invariant:
        //
        // A view is a `Console` if and only if it's `view->parent` is non-NULL
        // and `view->parent->console == view`.
        if (view->parent != NULL && (View *)view->parent->console == view) {
            // `view` is a console, and is already focused, so we close it.
            View_Close(view);
        } else if (view->console != NULL) {
            // `view` is not a console, but has one, so we show that.
            ViewManager_Focus(view->manager, (View *)view->console);
        } else if (view->console_program != NULL) {
            // `view` does not have an active console, but it has a program to
            // run, so we create a console to run the new program.
            uint32_t window_width, window_height;
            View_GetWindowSize(view, &window_width, &window_height);

            view->console = Console_New(
                view->manager, view,
                0,                      // x            (pixels)
                window_height,          // y            (pixels)
                80,                     // width        (columns)
                window_height/15,       // height       (rows)
                15,                     // font size    (height in pixels)
                view->console_program,
                view->console_state
            );

            ViewManager_Focus(view->manager, (View *)view->console);
        } else {
            // `view` is not a console, so technically we should display it's
            // console as a sub-view. However, it hasn't given us a program to
            // run, so we do nothing.
        }

        return;
            // The "toggle console" event is processed, we are done. Returning
            // here is especially important since processing the event may have
            // resulted in closing the view, in which case the rest of this
            // function is going to be nonsense.
    }

    // This is not a "toggle console" event, it is just a normal keyboard event.
    // Find a view above the focused view with a handler for the event.
    while (view != NULL) {
        if (view->key_callback) {
            view->key_callback(view, key, action, mods);
            return;
                // It's important that we return after running the handler, both
                // for performance (no point in continuing the search now that
                // we've handled the event) and for correctness, because the
                // handler may have closed the view, in which case continuing to
                // use it would be undefined behavior.
        }

        view = view->parent;
    }
}

static void ViewManager_CharCallback(GLFWwindow *window, uint32_t codepoint)
{
    // Get the manager from the window's user data.
    ViewManager *manager = glfwGetWindowUserPointer(window);
    ASSERT(manager != NULL);

    // Find a view above the focused one with a handler for this event.
    View *view = manager->focused;
    while (view != NULL) {
        if (view->character_callback) {
            view->character_callback(view, codepoint);
            return;
                // Return in case the handler closed `view`, making it invalid.
        }

        view = view->parent;
    }
}

static void ViewManager_MouseButtonCallback(
    GLFWwindow *window, int button, int action, int mods)
{
    // GLFW has support for fancy gamer mice with 8 mappable buttons. This is a
    // simple program which only cares about the three main buttons. If this
    // event was triggered by any of the other buttons, just drop it.
    if (button != GLFW_MOUSE_BUTTON_LEFT &&
        button != GLFW_MOUSE_BUTTON_RIGHT &&
        button != GLFW_MOUSE_BUTTON_MIDDLE) {
        return;
    }

    // Get the manager from the window's user data.
    ViewManager *manager = glfwGetWindowUserPointer(window);
    ASSERT(manager != NULL);

    // Find a view above the focused one with a handler for this event.
    View *view = manager->focused;
    while (view != NULL) {
        if (view->mouse_button_callback) {
            view->mouse_button_callback(view, button, action, mods);
            return;
                // Return in case the handler closed `view`, making it invalid.
        }

        view = view->parent;
    }
}

// GLFW does not have any support for a "drag" mouse event. This is a
// particularly useful kind of event for us, though, so we simulate it by
// triggering a drag event whenever
//  * GLFW triggers a "mouse moved" event, and
//  * At least one of the three main mouse buttons is pressed.
static void ViewManager_CursorPositionCallback(
    GLFWwindow *window, double x, double y)
{
    (void)x;
    (void)y;

    // Figure out which mouse buttons are pressed.
    bool left   = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)   == GLFW_PRESS;
    bool right  = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)  == GLFW_PRESS;
    bool middle = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;

    if (!left && !right && !middle) {
        // If no buttons are pressed, this is just a cursor move, not a drag.
        return;
    }

    // Get the manager from the window's user data.
    ViewManager *manager = glfwGetWindowUserPointer(window);
    ASSERT(manager != NULL);

    // Find a view above the focused one with a handler for this event.
    View *view = manager->focused;
    while (view != NULL) {
        if (view->mouse_button_callback) {
            // Figure out which modifier keys are pressed by polling the state
            // for each modifier that we care about.
            ModifierKey mods = 0;
            if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) {
                mods |= MOD_CONTROL;
            }
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
                mods |= MOD_SHIFT;
            }

            // Send a drag event for each button which is pressed. This may send
            // more than one event, but it will always send at least one, since
            // we short circuited earlier if none of the buttons were pressed.
            if (left) {
                view->mouse_button_callback(
                    view, MOUSE_BUTTON_LEFT, MOUSE_DRAG, mods);
            }
            if (right) {
                view->mouse_button_callback(
                    view, MOUSE_BUTTON_RIGHT, MOUSE_DRAG, mods);
            }
            if (middle) {
                view->mouse_button_callback(
                    view, MOUSE_BUTTON_MIDDLE, MOUSE_DRAG, mods);
            }
            return;
                // Return in case the handler closed `view`, making it invalid.
        }

        view = view->parent;
    }
}

////////////////////////////////////////////////////////////////////////////////
// ViewManager API
//

void ViewManager_Init(ViewManager *manager, GLFWwindow *window)
{
    manager->window = window;
    manager->roots = NULL;
    manager->focused = NULL;
    manager->last_time = Clock_GetTimeMS();

    // Set up all the window callbacks.
    glfwSetWindowUserPointer(window, manager);
    glfwSetCharCallback(window, ViewManager_CharCallback);
    glfwSetKeyCallback(window, ViewManager_KeyCallback);
    glfwSetMouseButtonCallback(window, ViewManager_MouseButtonCallback);
    glfwSetCursorPosCallback(window, ViewManager_CursorPositionCallback);
}

void ViewManager_Destroy(ViewManager *manager)
{
    View *view = manager->roots;
    ASSERT(view->parent == NULL);
    ASSERT(view->prev_sibling == NULL);

    // Traverse the list of top-level views and close them.
    while (view != NULL) {
        ASSERT(view->parent == NULL);
        View *next = view->next_sibling;
            // It's important that we get the next view before closing the
            // current one. Reading `view->next_sibling` after closing `view`
            // would be undefined behavior.
        View_Close(view);
        view = next;
    }
}

void View_UseProgram(View *view, const Command *program, void *state)
{
    view->console_program = program;
    view->console_state = state;
}

void ViewManager_Render(ViewManager *manager)
{
    uint64_t curr_time = Clock_GetTimeMS();
    ASSERT(curr_time >= manager->last_time);

    if(curr_time - manager->last_time < 10) {
        // We're running more than 100 frames per second, which is pointless.
        // Throttle back a little bit.
        Clock_SleepMS(10);
        curr_time = Clock_GetTimeMS();
    }

    if (manager->focused == NULL) {
        return;
    }

    ViewTraversal t = View_Traversal(View_Root(manager->focused));
    View *view;
    while ((view = View_Traverse(&t)) != NULL) {
        if (view->render != NULL) {
            view->render(view, curr_time - manager->last_time);
        }
    }

    glfwSwapBuffers(manager->window);
    manager->last_time = curr_time;
}

////////////////////////////////////////////////////////////////////////////////
// View API
//

View *View_New(size_t size, ViewManager *manager, View *parent)
{
    ASSERT(size >= sizeof(View));
    View *view = Malloc(size);

    // Parents
    view->manager = manager;
    view->parent = parent;

    // Children
    view->children = NULL;

    // Traversal
    view->queue_next = NULL;

    // Console
    view->console = NULL;
    view->console_program = NULL;
    view->console_state = NULL;

    // Callbacks
    view->render = NULL;
    view->destroy = NULL;
    view->key_callback = NULL;
    view->character_callback = NULL;
    view->mouse_button_callback = NULL;

    // Insert into parent's list of children, if parent is specified. Otherwise,
    // insert into manager's list of top-level views.
    View **siblings = parent ? &parent->children : &manager->roots;
        // `siblings` is a pointer to the pointer to the first node in `view`'s
        // sibling list. This will either be `parent`s `children` pointer, or
        // `manager`'s `roots` pointer, depending on whether or not this is a
        // top-level view.
        //
        // We get a pointer to this pointer so that we can modify it (we will
        // end up setting it to point at `view`) without another branch to check
        // if we should modify `parent->children` or `manager->roots`.
    view->prev_sibling = NULL;
    view->next_sibling = *siblings;
    if (*siblings != NULL) {
        (*siblings)->prev_sibling = view;
    }
    *siblings = view;

    return view;
}

void View_Focus(View *view)
{
    view->manager->focused = view;
}

void View_GetWindowSize(const View *view, uint32_t *width, uint32_t *height)
{
    int iwidth, iheight;
    glfwGetWindowSize(view->manager->window, &iwidth, &iheight);

    ASSERT(iwidth >= 0);
    ASSERT(iheight >= 0);

    *width = iwidth;
    *height = iheight;
}

void View_GetCursorPos(const View *view, int32_t *x, int32_t *y)
{
    double dx, dy;
    glfwGetCursorPos(view->manager->window, &dx, &dy);
    *x = floor(dx);
    *y = floor(dy);
}

void View_Close(View *view)
{
    // It is possible that `view` itself is the focused view, or, somewhat more
    // problematically, the focused view is a sub-view of `view`. If we
    // encounter the focused view while we are traversing sub-views of `view`
    // and freeing them, we will need to move the focus somewhere else.
    // `new_focused` will be a pointer to the view which should become focused
    // if we need to move the focus.
    View *new_focused = view->parent;
        // The simplest change we can make to the focus is to point it to the
        // parent of the view we're closing.
    if (new_focused == NULL) {
        new_focused = view->next_sibling;
            // If the closed view has no parent, we will instead move the focus
            // to the next top-level view.
    }

    // Remove `view` from it's parent's list of children.
    if (view->parent == NULL && view->manager->roots == view) {
        ASSERT(view->prev_sibling == NULL);
        view->manager->roots = view->next_sibling;
    } else if (view->parent != NULL && view->parent->children == view) {
        ASSERT(view->prev_sibling == NULL);
        view->parent->children = view->next_sibling;
    }
    if (view->next_sibling != NULL) {
        view->next_sibling->prev_sibling = view->prev_sibling;
    }
    if (view->prev_sibling != NULL) {
        view->prev_sibling->next_sibling = view->next_sibling;
    }

    // If `view` is a console, it's parent has a pointer to it through
    // `console`, so we need to close that.
    if (view->parent != NULL && (View *)view->parent->console == view) {
        view->parent->console = NULL;
    }

    // Traverse all the views in the sub-tree rooted at `view` and free them.
    ViewTraversal t = View_Traversal(view);
    while ((view = View_Traverse(&t)) != NULL) {
        if (view == view->manager->focused) {
            view->manager->focused = new_focused;
        }

        // If `view` is the root of the sub-tree we're closing, then we removed
        // it from its parent's list of children before entering this while
        // loop. Otherwise, by the time we reach `view` in the traversal, its
        // parent has been freed. In either case, `view` is not a member of any
        // view's `children` list.
        //
        // Additionally, if `view` has children, all of it's children will be
        // freed in the remainder of the traversal.
        //
        // Finally, if `view` was the focused view, we just moved the focus
        // elsewhere above.
        //
        // Therefore, when we finish the traversal, nothing will point to this
        // view, and so it is safe to free it.
        if (view->destroy != NULL) {
            view->destroy(view);
        }
        free(view);
    }
}
