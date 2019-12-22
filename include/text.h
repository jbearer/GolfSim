/**
 * \file text.h
 * \brief Facilities for text-based I/O through a GL-based GUI.
 *
 * Our text I/O facilities are divided into a hierarchy of three components,
 * each of which relates to the component below it through an "is a"
 * relationship.
 *
 * The three components are:
 *  * \ref TextField "TextField", a simple, output-only GUI component.
 *    `TextField` handles low-level rendering of text.
 *  * \ref TextInput "TextInput", a `TextField` which can also accept
 *    interactive input from the user via the keyboard. Input is handled via a
 *    callback, which is called by the `TextInput` whenever the user enters a
 *    new line.
 *  * \ref Console "Console", a `TextInput` which handles user input by parsing
 *    and executing commands, according to a programmable specification.
 *
 * The rationale for this division of components is twofold:
 *  1. Separation of concerns.
 *
 *     Each component in the hierarchy handles one simple task:
 *      * The `TextField` deals only with rendering text. It has no need for
 *        interactivity or for interpreting the characters it displays.
 *      * The `TextInput` deals only with processing keyboard input. It
 *        delegates to `TextField` for low-level rendering, and it has no need
 *        for interpreting input.
 *      * The `Console` deals only with interpreting and executing commands. It
 *        delegates to `TextInput` for low-level rendering and interacting with
 *        the keyboard.
 *
 *  2. Reusability.
 *
 *     Each component in the hierarchy is useful and necessary on its own. This
 *     game needs simple, output-only text fields. It needs text-based inputs.
 *     And it absolutely needs a console for processing commands. This is
 *     perhaps the most important component in the hierarchy, because it means
 *     the application can be debugged even if the GUI is acting up.
 */

#include <stdbool.h>
#include <stdint.h>

#include <GLFW/glfw3.h>

#include "matrix.h"
#include "pp.h"
#include "view.h"

/**
 * \defgroup TextField TextField: Output-only text rendering
 * @{
 */

typedef struct {
    View view;

    uint16_t x;         // Horizontal position of the top left corner of the
                        // component.
    uint16_t y;         // Vertical position of the top left corner of the
                        // component.
    uint8_t width;      // Number of characters per line
    uint8_t height;     // Number of lines
    uint8_t font_size;  // Character height in view units

    uint8_t cursor_x;   // Horizontal position of the cursor, in characters
    uint8_t cursor_y;   // Vertical position of the cursor, in lines. This will
                        // usually be `height`, as once the buffer fills up we
                        // will only append to the bottom. However, when we
                        // first create the text field, it starts out empty, and
                        // we start appending from the top.
    bool show_cursor;

    char *buffer;       // width x height array of characters being displayed.


    // View transformations
    mat3 transform;
        // We only use one matrix to represent the full MVP transform for the
        // text box. This matrix represents a transform from GUI coordinates
        // (where the origin is in the lower left and the dimensions of the view
        // correspond to pixels in the window) to 2- dimensional GL clipping
        // coordinates (where the origin is in the center and the dimensions of
        // the view are 2x2).

    vec4 fg_color;
    vec4 bg_color;

    // GL stuff
    GLuint vao;
    GLuint vertex_positions;
    GLuint vertex_uv;
    GLuint vertex_cursor;
    GLuint shaders;
    GLuint font_sampler;
    GLuint font_texture;
    GLuint mvp;
    GLuint shader_fg_color;
    GLuint shader_bg_color;
} TextField;

/**
 * \brief Allocate and initialize a new `TextField`.
 *
 * \param size       The size of the object to create. If this constructor is
 *                   being used to create the TextField portion of a derived
 *                   class (e.g. TextInput or Console), this should be the size
 *                   of that object (e.g. `sizeof(TextInput)` or
 *                   `sizeof(Console)`). Otherwise, this should be
 *                   `sizeof(TextField)`.
 * \param manager    The manager for the  window where the text field will be
 *                   displayed.
 * \param parent     Parent view which will contain the text field.
 * \param x          The distance in pixels from the left edge of the window to
 *                   the top-left corner of the text field.
 * \param y          The distance in pixels from the bottom edge of the window
 *                   to the top-left corner of the text field.
 * \param width      The number of columns in the text field.
 * \param height     The number of rows.
 * \param font_size  The height of a character in pixels.
 *
 * The text field is created unfocused. If it should be focused (often desired
 * for text inputs) it may be explicitly focused after creation by calling
 * `View_Focus`.
 *
 * \pre `size >= sizeof(TextField)`
 */
TextField *TextField_New(size_t size, ViewManager *manager, View *parent,
    uint16_t x, uint16_t y, uint8_t width, uint8_t height, uint8_t font_size);

/**
 * \brief Set the position of the top left corner of the text field.
 */
void TextField_SetLocation(TextField *text_field, uint16_t x, uint16_t y);

/**
 * \brief Set the foreground color of the text field.
 *
 * \param text_field The text field to update.
 * \param color      The foreground color in RGBA format.
 *
 * The foreground color is used to render deselected characters, as well as the
 * background of selected cells.
 */
void TextField_SetForegroundColor(TextField *text_field, const vec4 *color);

/**
 * \brief Set the background color of the text field.
 *
 * \param text_field The text field to update.
 * \param color      The background color in RGBA format.
 *
 * The background color is used to render the background of deselected cells, as
 * well as the characters in selected cells.
 */
void TextField_SetBackgroundColor(TextField *text_field, const vec4 *color);

/**
 * \brief Enable rendering of the cursor position in the text field.
 *
 * Cursor rendering is disabled by default. This turns it on. After a call to
 * `TextField_ShowCursor`, until a corresponding call to `TextField_HideCursor`,
 * the cell containing the cursor will be rendered in a manner that is visually
 * distinct from cells not containing the cursor.
 */
void TextField_ShowCursor(TextField *text_field);

/**
 * \brief Disable rendering of the cursor position in the text field.
 *
 * Cursor rendering is disabled by default, and this function is a no-op if
 * rendering is still disabled. If rendering has been enabled by a corresponding
 * call to `TextField_ShowCursor`, this function disables cursor rendering,
 * restoring cursor rendering to its initial state.
 */
void TextField_HideCursor(TextField *text_field);

/**
 * \brief Set the cursor position within a line.
 *
 * The cursor moves to the column `cursor` within the current line. (There is no
 * inerface for changing the line containing the cursor.)
 *
 * \pre
 * `cursor` must be less than the `TextField_GetWidth(text_field)`.
 */
void TextField_SetCursor(TextField *text_field, uint8_t cursor);

/**
 * \brief Move the cursor horizontally within a line.
 *
 * If the new cursor position is within range (that is,
 * `0 <= TextField_GetCursor(cursor) + delta < TextField_GetWidth(text_field)`)
 * then this function behaves as
 * `TextField_SetCursor(text_field, TextField_GetCursor(text_field) + delta)`.
 *
 * If the new cursor position is less than 0, the cursor position is set to 0.
 * If the new cursor position is greater than or equal to the width of the text
 * field, the cursor position is set to `TextField_GetWidth(text_field) - 1`.
 */
void TextField_MoveCursor(TextField *text_field, int16_t delta);

/**
 * \brief Get the current horizontal cursor position.
 */
uint8_t TextField_GetCursor(const TextField *text_field);

/**
 * \brief Return the value of the `width` paramter passed to `TextField_Init`.
 */
uint8_t TextField_GetWidth(const TextField *text_field);

/**
 * \brief Insert a character at the current cursor position.
 *
 * If the current horizontal cursor position is within the width of a line, the
 * character at that position is set to `c`, and the cursor is moved one
 * character to the right.
 *
 * If the current cursor position is passed the end of the line, no update is
 * performed. (`TextField` does not support any kind of horziontal scrolling).
 *
 * If `c` is `'\n'`, the cursor is advanced to the next line (scrolling
 * vertically if necessary) and the horizontal position of the cursor is reset
 * to 0. The text field is also flushed, as if by calling `TextField_Flush`.
 */
void TextField_PutChar(TextField *text_field, char c);

/**
 * \brief Insert a string at the current cursor position.
 *
 * This function behaves as calling `TextField_PutChar` on each character in
 * `string`. If `string` does not end in a newline, the output may be buffered.
 * Buffered output will not be rendered to the screen when `TextField_Render` is
 * called, unless `TextField_Flush` is called first.
 */
void TextField_PutString(TextField *text_field, const char *string);

/**
 * \brief Insert a line at the current cursor position.
 *
 * This function behaves as calling `TextField_PutString(text_field, line)`, and
 * then calling `TextField_PutChar(text_field, '\n')`.
 */
void TextField_PutLine(TextField *text_field, const char *line);

/**
 * \brief Print formatted text to a text field.
 *
 * The format string `fmt` is interpreted as a `printf`-style format string. The
 * caller must supply variadic arguments of the correct type for each format
 * specifier in the string. A formatted string is constructed by inserting the
 * string value of each argument into the specified place in the format string.
 * This string is then appended to the current output line.
 *
 * If a newline is encountered during formatting, the buffer is flushed and the
 * remainder of the formatted string is inserted starting in the next line, as
 * if the newline character had been passed to `TextField_PutChar`. If the
 * length of the formatted string exceeds the remaining space on the current
 * line before a newline is encountered, output is truncated at that point.
 *
 * \todo Newline characters in the middle of the output string are ignored.
 */
void TextField_Printf(TextField *text_field, const char *fmt, ...);

/**
 * \brief Flush buffered output.
 *
 * After calling this function, the results of all previous calls to
 * `TextField_PutChar`, `TextField_PutString`, and `TextField_PutLine` will be
 * accurately rendered to the screen at the next call to `TextField_Render`.
 */
void TextField_Flush(TextField *text_field);

/**
 * @}
 *
 * \defgroup TextInput TextInput: Input-output text I/O
 * @{
 */

typedef struct TextInput TextInput;

typedef void (*TextInputCallback)(TextInput *input, char *line);

struct TextInput {
    TextField text_field;           ///< Inherits from `TextField`.
    TextInputCallback handle_line;  ///< Action to take after receiving a line
                                    ///< of input.
    View_DestroyFunction super_destroy;
                                    ///< Destroy the `TextField` portion of this
                                    ///< object.
    const char *prompt;             ///< Prompt to be displayed before each line.
    char *buffer;                   ///< Input buffer.
    uint8_t num_buffered;           ///< Number of valid characters in `buffer`.
};

/**
 * \brief Initialize a new `TextInput`.
 *
 * Delegates to `TextField_New()`.
 *
 * \param handle_line Callback which will be called after receiving a line of
 *                    input. The callback takes as arguments the `TextInput`
 *                    object and the line to process. The callback may modify
 *                    the line in place; the `TextInput` object will not inspect
 *                    the contents of the line buffer after calling the
 *                    callback. However, the `TextInput` may reuse the buffer
 *                    modify its contents later, so if the callback wishes to
 *                    maintain the processed line, it should allocate a copy.
 *
 * \pre `size >= sizeof(TextInput)`
 */
TextInput *TextInput_New(
    size_t size, ViewManager *manager, View *parent, uint16_t x, uint16_t y,
    uint8_t width, uint8_t height, uint8_t font_size,
    TextInputCallback handle_line);

/**
 * \brief Set the prompt which is displayed before each line of input.
 *
 * By default, there is no prompt.
 *
 * \pre
 * `prompt` points to a statically allocated, constant, null-terminated string.
 */
void TextInput_SetPrompt(TextInput *text_input, const char *prompt);

/**
 * @}
 *
 * \defgroup Console Console: A programmable command line interpreter
 * @{
 */

typedef struct Command Command;

typedef struct Console {
    TextInput text_input;
    const Command *program;
    void *state;
} Console;

/**
 * \brief Allocate and initialize a new `Console`.
 *
 * Delegates to `TextInput_New()`.
 *
 * \param program The program to use to interpret command line intput.
 * \param state   State to pass to each command invocation. Must point to an
 *                object of type `PROG_STATE_TYPE`, specified via
 *                `PROGRAM_INFO`.
 *
 * \pre
 * `program` must be the name of a program specified via `PROGRAM_INFO` and
 * declared with `DECLARE_PROGRAM`.
 */
Console *Console_New(
    ViewManager *manager, View *parent, uint16_t x, uint16_t y,
    uint8_t width, uint8_t height, uint8_t font_size,
    const Command *program, void *state);

/**
 * \brief Execute commands in a file using the given console.
 *
 * \param script_path   Name of the file containing the script to run.
 * \param echo          Enable printing of executed commands to the console.
 *
 * Each line in `script_path` is treated as a command. Empty lines and lines
 * starting with '#' are ignored. The rest of the lines are processed as if they
 * had been entered by the user into the console.
 *
 * If `script_path` does not exist, or otherwise cannot be opened, an error is
 * printed to the console and no commands are processed.
 */
void Console_RunScript(Console *console, const char *script_path, bool echo);

/**
 * \defgroup Console_Programming Programming a `Console`
 *
 * Console's are programmed using an internal DSL that allows the programmer to
 * create `Command`s. These commands can then be invoked by the user from the
 * console. The syntax used to invoked a command is reminiscient of a UNIX-style
 * shell:
 *
 * ```
 * <base-command> [<sub-commands> ...] [<args> ...]
 * ```
 *
 * This syntax induces a tree structure in the commands available in a given
 * program: each `base-commmand` may have several sub-commands, which in turn
 * may have their own sub-commands. At a certain point, a string of sub-commands
 * specifies a "leaf" command -- one with no more subcommands. This "leaf"
 * command dictates the action which is taken to handle this command.
 *
 * The programmer begins a console program by defining a macro `PROGRAM_INFO`
 * with information about the program being defined. Program information is
 * specified as a series of key-value pairs, with the following form:
 *
 * ```
 * #define PROGRAM_INFO(info) \
 *      info(PROG_ID, id)
 * ```
 *
 * The `PROG_ID` key must be specified. The value must be a valid C identifier
 * which will be used to refer to the declared program.
 *
 * Additional key-value pairs may optionally be passed as subsequent calls to
 * `info` to further specify the program. Allowed options are:
 *  * PROG_STATE_TYPE [default: void]
 *      the type of program-global state which will be available to each command
 *      executed by the program.
 *  * PROG_STATE_NAME [default: state]
 *      the name by which program commands can refer to the global state.
 *
 * After defining `PROGRAM_INFO`, the programmer may begin to provide code to
 * handle each leaf command via `DECLARE_RUNNABLE()`. They can register existing
 * commands (leaf or otherwise) as sub-commands of a new base command using
 * `DECLARE_SUB_COMMANDS()`. Leaf commands can also be used themselves as base
 * commands. Finally, `DECLARE_PROGRAM()` can be used to list the top-level
 * commands which will make up a program.
 *
 * To close out the definition of a program:
 * ```
 * #undef PROGRAM_INFO
 * ```
 *
 * @{
 */

/**
 * \brief Declare a command with a callback function.
 *
 * \param id        An identifier for the new command.
 * \param name_str  A human-readable name for the new command.
 * \param help_str  A brief description of the command.
 *
 * Takes a brace-enclosed block of code to be exected when the command is
 * invoked. Inside the block, the following implicit parameters are in scope:
 *
 * \param console   A pointer to the `Console` object with which this command
 *                  was invoked.
 * \param state     A pointer to the program-global state which was passed to
 *                  `Console_Init()`. (Note: if `info(PROG_STATE_NAME, name)`
 *                  was specified in the declaration of `PROGRAM_INFO`, the name
 *                  of this parameter will be `name` instead of `state`.)
 * \param argc      The number of positional arguments passed to the command.
 * \param argv      A pointer to an array of null-terminated strings,
 *                  representing the positional arguments passed to the command.
 *
 * Example:
 * ```
 * DECLARE_RUNNABLE(hello, "hello", "say hello")
 * {
 *      TextField_PutLine((TextField *)console, "hello!");
 * }
 *
 * DECLARE_RUNNABLE(goodbye, "bye", "say goodbye")
 * {
 *      TextField_PutLine((TextField *)console, "bye!");
 * }
 *
 * DECLARE_RUNNABLE(how_are_you, "sup", "ask what's up")
 * {
 *      TextField_PutLine((TextField *)console, "what's up?");
 * }
 *
 * ```
 */
#define DECLARE_RUNNABLE(id, name_str, help_str) \
    PROG_RUNNABLE_PROTOTYPE(id); \
    static Command id = { \
        .name = name_str, \
        .help = help_str, \
        .type = RUN_COMMAND, \
        .impl = { \
            .run = (CommandRunner)PROG_RUNNABLE_NAME(id) \
        } \
    }; \
    PROG_RUNNABLE_PROTOTYPE(id)

/**
 * \brief Declare a command with a list of subcommands.
 *
 * \param id            An identifier for the new command.
 * \param name_str      A human-readable name for the new command.
 * \param help_str      A brief description of the command.
 * \param __VA_ARGS__   A series of pointers to the sub-commands of this
 *                      command. Each sub-command must point to a command
 *                      declared with `DECLARE_RUNNABLE` or
 *                      `DECLARE_SUB_COMMANDS`.
 *
 * Example:
 * ```
 * DECLARE_SUB_COMMANDS(say, "say", "say something", &hello, &goodbye);
 * DECLARE_SUB_COMMANDS(ask, "ask", "ask something", &how_are_you);
 * ```
 */
#define DECLARE_SUB_COMMANDS(id, name_str, help_str, ...) \
    static Command *PROG_SUB_COMMANDS_NAME(id)[] = { __VA_ARGS__ }; \
    static Command id = { \
        .name = name_str, \
        .help = help_str, \
        .type = SUB_COMMANDS, \
        .impl = { \
            .sub_commands = { \
                .num_commands = sizeof(PROG_SUB_COMMANDS_NAME(id)) \
                              / sizeof(PROG_SUB_COMMANDS_NAME(id)[0]), \
                .commands = (Command *const *)PROG_SUB_COMMANDS_NAME(id), \
            } \
        }, \
    }

/**
 * \brief Associate a list of top-level commands with the active program.
 *
 * The arguments to `DECLARE_PROGRAM` should be a series of pointers to command
 * identifiers declared via `DECLARE_RUNNABLE` or `DECLARE_SUB_COMMANDS`.
 *
 * Example:
 * ```
 * DECLARE_PROGRAM(converse, &say, &ask);
 * ```
 */
#define DECLARE_PROGRAM(...) DECLARE_SUB_COMMANDS( \
    PROG_CHECK_PROGRAM_INFO(PROG_ID), NULL, NULL, __VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////
// Internal helpers for command-line DSL
//
#ifndef DOXYGEN

// Type of functions which handle execution of a leaf command. Handlers get four
// arguments:
//  * Console where the command was entered.
//  * Program-global state passed to `Console_Init`.
//  * Number of positional arguments given by the user.
//  * Array of positional argumens.
typedef void(*CommandRunner)(
    Console *console, void *state, int argc, const char **argv);

// Internal representation of a command.
struct Command {
    ////////////////////////////////////////////////////////////////////////////
    // Header common to all command types.
    //
    const char *name;
        // Name of the command. This is the string used to specify the command
        // from the console.
        //
        // `name` may be NULL to indicate that this is the root command of the
        // program, of which all programmer-specified base commands are
        // sub-commands. Having such a root command lets us represent the
        // program structure as a tree, rather than a forest, which would be a
        // bit messier.
        //
        // The justification for `name` of the root command being NULL is that
        // the root command is always present implicitly at the beginning of any
        // command entered by the user; or, equivalently, the root command is
        // named by the empty string.
    const char *help;
        // Help string to display when handling `help <name>`.
    const Command *parent;
        // Pointer to the parent command of which this is a sub-command.
        //
        // Most commands are declared statically, and it is not possible to
        // create a pointer cycle in C at static initialization time without
        // forward declarations, which would make the Command DSL cumbersome. To
        // work around this problem, we initialize the parent pointers lazily.
        // When a `Command` is declared statically, its parent pointer will
        // always be `NULL`. It will remain that way even if a new command is
        // later declared with the first command as a sub-command. The firt time
        // the sub-command is inspected at run-time (for example, parsing the
        // console line `command sub-commad`) it's parent pointer will be
        // updated if necessary to point to its parent command.

    ////////////////////////////////////////////////////////////////////////////
    // Tag indicating what kind of command this is.
    //
    enum {
        RUN_COMMAND,    // Leaf command
        SUB_COMMANDS,   // Parent command
    } type;

    ////////////////////////////////////////////////////////////////////////////
    // Type-specific data.
    //
    union {
        ////////////////////////////////////////////////////////////////////////
        // Leaf data.
        //
        CommandRunner run;
            // The callback defined with `DECLARE_RUNNABLE`, used to execute
            // this command when it is specified by the user as a leaf command.

        ////////////////////////////////////////////////////////////////////////
        // Parent data.
        //
        struct {
            uint8_t num_commands;
                // Number of sub-commands under this command.
            Command *const *commands;
                // Pointer to an array of `num_commands` pointers to the sub-
                // commands of othis command.
        } sub_commands;
    } impl;
};

////////////////////////////////////////////////////////////////////////////////
// Using PROGRAM_INFO
//

#define PROG_EXTRACT_PROGRAM_INFO(key, tag, value) \
    PP_IF(PP_TOKEN_EQ(key, tag), value,)
    // Extract value for a target key from a given PROGRAM_INFO key-value pair
    // `(tag, value)`. If `key` matches `tag` according to `PP_TOKEN_EQ`, then
    // this macro expands to `value`. Otherwise, it expands to the empty token
    // sequence.

#define PROG_EXPAND_PROGRAM_INFO(key) \
    PROGRAM_INFO(PP_CAT(PROG_EXPAND_PROGRAM_INFO_, key))
    // Expand the value(s) specified for `key` in the current definition of
    // PROGRAM_INFO. This macro expands to a token sequence consisting of the
    // value `v` for each pair `(key, v)` specified in the current definition of
    // `PROGRAM_INFO`.
    //
    // It is implemented by using `PROGRAM_INFO` to evaluate
    // `PROG_EXPAND_PROGRAM_INFO_key` on each key-value pair, which expands to
    // the associated value for pairs whose key matches `key` and nothing for
    // non- matching pairs.

////////////////////////////////////////////////////////////////////////////////
// Declarations for PROGRAM_INFO keys.
//
// For each new key, the following macros must be defined:
//  * #define PP_COMPARE_key(x) x
//  * #define PROG_EXPAND_PROGRAM_INFO_key(tag, value) \                      //
//          PROG_EXTRACT_PROGRAM_INFO(key, tag, value)
//

#define PP_COMPARE_PROG_ID(x) x
#define PROG_EXPAND_PROGRAM_INFO_PROG_ID(tag, value) \
    PROG_EXTRACT_PROGRAM_INFO(PROG_ID, tag, value)

#define PP_COMPARE_PROG_STATE_TYPE(x) x
#define PROG_EXPAND_PROGRAM_INFO_PROG_STATE_TYPE(tag, value) \
    PROG_EXTRACT_PROGRAM_INFO(PROG_STATE_TYPE, tag, value)

#define PP_COMPARE_PROG_STATE_NAME(x) x
#define PROG_EXPAND_PROGRAM_INFO_PROG_STATE_NAME(tag, value) \
    PROG_EXTRACT_PROGRAM_INFO(PROG_STATE_NAME, tag, value)

// Get the value associated with `key` by the current definition of
// `PROGRAM_INFO`, or `def` if `key` is not specified.
#define PROG_GET_PROGRAM_INFO(key, def) \
    PP_IF(PP_IS_EMPTY(PROG_EXPAND_PROGRAM_INFO(key)), \
        def, \
        PROG_EXPAND_PROGRAM_INFO(key))

// Get the value associated with `key` by the current definition of
// `PROGRAM_INFO`, producing a compile-time error if `key` is not specified.
#define PROG_CHECK_PROGRAM_INFO(KEY) \
    PROG_GET_PROGRAM_INFO(KEY, PP_ERROR(program info key KEY must be specified))

////////////////////////////////////////////////////////////////////////////////
// Codegen helpers for the program-creating macros.
//

#define PROG_RUNNABLE_NAME(id) PP_CAT(id, _run)
    // Name for generated leaf handler functions.
#define PROG_SUB_COMMANDS_NAME(id) PP_CAT(id, _sub_commands)
    // Name for generated sub-command arrays.

#define PROG_RUNNABLE_PROTOTYPE(id) \
    /* Prototype for generated leaf handler functions. */ \
    static void PROG_RUNNABLE_NAME(id)( \
        Console *console, \
        PROG_GET_PROGRAM_INFO(PROG_STATE_TYPE, void)  \
            *PROG_GET_PROGRAM_INFO(PROG_STATE_NAME, state), \
        int argc, const char **argv)

#endif // DOXYGEN

/**
 * @}
 * @}
 */
