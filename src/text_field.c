#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "errors.h"
#include "gl.h"
#include "matrix.h"
#include "text.h"

////////////////////////////////////////////////////////////////////////////////
// Monofur bitmap font parameters
//
// The font bitmap is layed out like:
//
//      abcdefghijklmnopqrstuvwxyz
//      ABCDEFGHIJKLMNOPQRSTUVWXYZ
//      0123456789.:,;(*!?}^)#${%^&-+@
//
// In ASCII, that's:
//
//      0x61 - 0x7A
//      0x41 - 0x5A
//      0x30 - 0x39 ...
//
// where '...' (the punctuation characters) are scattered through the alphabet.
//
// Because the letters and numbers are laid out nicely with respect to their
// ASCII encodings, and because the font is monospace, we only need to know the
// coordinates of the first character in each of the three ranges of contiguous
// ASCII encodings (lower-case, upper-case, and numbers) to compute the
// coordinates of any character in that range.
//
// Because the punctuation marks are not laid out nicely, we will need to hard-
// code the coordinates of any punction marks we care about being able to
// render.
//
// All coordinates and sizes of characters in the font will be represented in
// texture-space:
//
//    (0, 1)           (1, 1)
//          .---------.
//          |         |
//          |         |
//          |         |
//          .---------.
//    (0, 0)           (1, 0)
//
// All coordinates will refer to the lower-left corner of the character they
// represent, so that the bounding box of the character with coordinates (u, v)
// is { (u, v + FONT_HEIGHT),   (u + FONT_WIDTH, v + FONT_HEIGHT),
//      (u, v),                 (u + FONT_WIDTH, v) }.
//

// Size in pixels of a single character in the font bitmap.
static const float FONT_WIDTH = 0.0175;
static const float FONT_HEIGHT = 0.037;

// Texture-space coordinates for 'a'.
#define FONT_LOWER_A ((vec2) { 0.02, 0.955 })

// Texture-space coordinates for 'A'.
#define FONT_UPPER_A ((vec2) { FONT_LOWER_A.x, FONT_LOWER_A.y - FONT_HEIGHT})

// Texture-space coordinates for '0'.
#define FONT_0 ((vec2) { FONT_UPPER_A.x, FONT_UPPER_A.y - FONT_HEIGHT})

// Texure-space coordinates for ' '
#define FONT_SPACE ((vec2) { 0, 0 })

// Texture-space coordinates for the start of the punctuation characters.
#define FONT_PUNCTUATION ((vec2) { FONT_0.x + 10*FONT_WIDTH, FONT_0.y })

// Punctuation characters as they appear in the font bitmap.
static const char *FONT_PUNCTUATION_CHARS = ".:,;(*!?}^)#${%^&-+@";

// Texture-space coordinates for any ASCII character.
static void Font_Coords(char c, vec2 *v)
{
    if ('a' <= c && c <= 'z') {
        v->x = FONT_LOWER_A.x + (FONT_WIDTH*(c - 'a'));
        v->y = FONT_LOWER_A.y;
    } else if ('A' <= c && c <= 'Z') {
        v->x = FONT_UPPER_A.x + (FONT_WIDTH*(c - 'A'));
        v->y = FONT_UPPER_A.y;
    } else if ('0' <= c && c <= '9') {
        v->x = FONT_0.x + (FONT_WIDTH*(c - '0'));
        v->y = FONT_0.y;
    } else if (c == ' ') {
        *v = FONT_SPACE;
    } else if (c == '\'') {
        // The font doesn't have an apostrophe, but it's a pretty important
        // character, so we hack it by using a comma shifted up.
        char *p = strchr(FONT_PUNCTUATION_CHARS, ',');
        ASSERT(p != NULL);

        uintptr_t index = p - FONT_PUNCTUATION_CHARS;
        v->x = FONT_PUNCTUATION.x + (FONT_WIDTH*index);
        v->y = FONT_PUNCTUATION.y - (FONT_HEIGHT/2);
    } else {
        char *p = strchr(FONT_PUNCTUATION_CHARS, c);
        if (p == NULL) {
            warn("Tried to render unprintable character %#x\n", (int)c);
            p = strchr(FONT_PUNCTUATION_CHARS, '?');
                // Can't print this character, print a question mark instead.
        }
        ASSERT(p != NULL);

        uintptr_t index = p - FONT_PUNCTUATION_CHARS;
        v->x = FONT_PUNCTUATION.x + (FONT_WIDTH*index);
        v->y = FONT_PUNCTUATION.y;
    }
}

#define FONT_ASPECT 0.6

// Get a pointer to the character at `(row, col)` in the output buffer.
static char *TextField_CharAt(TextField *text_field, uint8_t row, uint8_t col)
{
    return &text_field->buffer[text_field->width*row + col];
}

static void TextField_Render(View *view_base, uint32_t dt)
{
    (void)dt;
    TextField *text_field = (TextField *)view_base;

    // Load the font texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, text_field->font_texture);
    glUniform1i(text_field->font_sampler, 0);

    glUseProgram(text_field->shaders);
    glUniformMatrix3fv(
        text_field->mvp, 1, GL_TRUE, mat3_ConstBuffer(&text_field->transform));
    glUniform4fv(
        text_field->shader_fg_color, 1, vec4_ConstBuffer(&text_field->fg_color));
    glUniform4fv(
        text_field->shader_bg_color, 1, vec4_ConstBuffer(&text_field->bg_color));

    glBindVertexArray(text_field->vao);
    {
        glDrawArrays(GL_TRIANGLES, 0, 6*text_field->width*text_field->height);
    }
    glBindVertexArray(0);
}

static void TextField_Destroy(View *view_base)
{
    TextField *text_field = (TextField *)view_base;
    free(text_field->buffer);
}

TextField *TextField_New(
    size_t size, ViewManager *manager, View *parent,
    uint16_t x, uint16_t y,
    uint8_t width, uint8_t height,
    uint8_t font_size)
{
    ASSERT(size >= sizeof(TextField));
    TextField *text_field = (TextField *)View_New(size, manager, parent);
    View_SetRenderCallback((View *)text_field, &TextField_Render);

    // Location and size fields.
    text_field->x = x;
    text_field->y = y;
    text_field->width = width;
    text_field->height = height;
    text_field->font_size = font_size;

    // Cursor fields.
    text_field->cursor_x = 0;
    text_field->cursor_y = 0;
    text_field->show_cursor = false;

    // Initialize the transformation matrix. We need to go from window
    // coordinates:
    //
    //                     window width (px)
    //                  <------------------->
    //                y
    //                  ^--------------------    ^
    //                  |                   |    |
    //                  |                   |    |
    //                  |                   |    | window height (px)
    //                  |                   |    |
    //                  |                   |    |
    //                  o------------------->    V
    //                                       x
    //
    // To view coordinates:
    //
    //                      -1          1
    //                  <--------| |-------->
    //                           y
    //                  ----------^----------    ^
    //                  |         |         |    | 1
    //                  |         |         |    _
    //                  |         o--------->    _
    //                  |                   | x  |
    //                  |                   |    | -1
    //                  ---------------------    V
    //
    // This transformation requires first scaling by (2/width, 2/height) to
    // normalize x- and y-coordinates to the range [0, 2], and then translating
    // by (-1, -1) to move the origin to (-1, -1), where it will be rendered in
    // the bottom left corner of the screen, like we want.
    uint32_t window_width, window_height;
    View_GetWindowSize((View *)text_field, &window_width, &window_height);
    mat3_Copy(&text_field->transform, &I3);
        // Initialize to the identity so we can layer transformations on.
    mat3 m;
    vec2 v;
    v = (vec2) { 2.0/window_width, 2.0/window_height };
    mat3_Scale(&m, &v);
    mat3_ComposeInPlace(&m, &text_field->transform);
        // Scale by (2/width, 2/height).
    v = (vec2) { -1, -1 };
    mat3_Translation(&m, &v);
    mat3_ComposeInPlace(&m, &text_field->transform);
        // Translate by (-1, -1).
    trace("Console transform:\n%s", mat3_String(&text_field->transform));

    // Initialize GL fields.
    text_field->shaders = GL_LoadShaders(
        "shaders/text_vertex.glsl", "shaders/text_fragment.glsl");
    text_field->font_texture = GL_LoadTexture("textures/monofur.bmp");
    text_field->font_sampler = glGetUniformLocation(text_field->shaders, "font");
    text_field->mvp = glGetUniformLocation(text_field->shaders, "mvp");
    text_field->shader_fg_color = glGetUniformLocation(text_field->shaders, "fg_color");
    text_field->shader_bg_color = glGetUniformLocation(text_field->shaders, "bg_color");

    // By default, set the foreground color to white and the background to a
    // dark gray color.
    vec4 color = { 1, 1, 1, 1};
    TextField_SetForegroundColor(text_field, &color);
    color = (vec4) { 0, 0, 0, 0.4 };
    TextField_SetBackgroundColor(text_field, &color);

    // Create vertex buffers.
    glGenVertexArrays(1, &text_field->vao);
    glGenBuffers(1, &text_field->vertex_positions);
    glGenBuffers(1, &text_field->vertex_uv);
    glGenBuffers(1, &text_field->vertex_cursor);

    // Initialize the vertex position buffer. This never changes after
    // initialization, so we do it here rather than in TextField_Flush.
    glBindVertexArray(text_field->vao);
    {
        glBindBuffer(GL_ARRAY_BUFFER, text_field->vertex_positions);
        {
            uint8_t char_height = text_field->font_size;
            uint8_t char_width = FONT_ASPECT*char_height;

            uint32_t num_vertices = 6*text_field->width*text_field->height;
                // Each character slot is a quadrilateral, or two triangles, so
                // requires 6 vertices.

            vec2 *positions = Malloc(num_vertices*sizeof(vec2));

            // Initialize positions
            uint16_t i = 0;
                // Offset of the current vertex in `positions`.
            for (uint8_t row = 0; row < text_field->height; ++row) {
                for (uint8_t col = 0; col < text_field->width; ++col) {
                    // Coordinates of the top-left corner of this character.
                    uint16_t x = text_field->x + col*char_width;
                    uint16_t y = text_field->y - row*char_height;

                    // Set up coordinates for the following two triangles:
                    //
                    //               (x, y)        (x + char_width, y)
                    //                     A-----,B
                    //                     | 1  / |
                    //                     |  ,`  |
                    //                     | /  2 |
                    //                     C------D
                    // (x, y - char_height)        (x + char_width, y - char_height)

                    // Triangle 1: top left, ABC
                    positions[i++] = (vec2){ x,             y };
                    positions[i++] = (vec2){ x+char_width,  y };
                    positions[i++] = (vec2){ x,             y-char_height };

                    // Triangle 2: bottom right, DBC
                    positions[i++] = (vec2){ x+char_width, y-char_height };
                    positions[i++] = (vec2){ x+char_width, y };
                    positions[i++] = (vec2){ x,            y-char_height };
                }
            }

            glBufferData(
                GL_ARRAY_BUFFER,
                num_vertices*sizeof(vec2),
                positions,
                GL_STATIC_DRAW
            );
            glVertexAttribPointer(
                VERTEX_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(VERTEX_ATTRIB_POSITION);

            free(positions);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    glBindVertexArray(0);

    // Create empty output buffer.
    text_field->buffer = Malloc(text_field->width*text_field->height);
    memset(text_field->buffer, ' ', text_field->width*text_field->height);

    // All of our resources are allocated, set up a destroy function to release
    // them when the view is closed.
    View_SetDestroyCallback((View *)text_field, TextField_Destroy);

    // Initialize character vertex data.
    TextField_Flush(text_field);

    return text_field;
}

void TextField_SetForegroundColor(TextField *text_field, const vec4 *color)
{
    text_field->fg_color = *color;
}

void TextField_SetBackgroundColor(TextField *text_field, const vec4 *color)
{
    text_field->bg_color = *color;
}

static void TextField_Scroll(TextField *text_field)
{
    // Move the cursor to the start of the next row.
    text_field->cursor_x = 0;
    text_field->cursor_y++;

    if (text_field->cursor_y >= text_field->height) {
        // Scroll down one row. For each row up to (not including) the last one,
        // we copy the contents of the next row into the current row.
        for (uint8_t row = 0; row + 1 < text_field->height; ++row) {
            memcpy(TextField_CharAt(text_field, row, 0),
                   TextField_CharAt(text_field, row+1, 0),
                   text_field->width);
        }

        // The bottom row is now empty.
        memset(TextField_CharAt(text_field, text_field->height - 1, 0),
               ' ', text_field->width);

        // All the lines have moved up one row, so we move the cursor up.
        --text_field->cursor_y;
    }
}

void TextField_PutChar(TextField *text_field, char c)
{
    if (c == '\n') {
        TextField_Scroll(text_field);
        TextField_Flush(text_field);
        return;
    }

    if (text_field->cursor_x >= text_field->width) {
        warn("Attempting to call PutChar(%c) when the current line is full."
             "The new character will not be rendered.\n", c);
        return;
    }

    *TextField_CharAt(
        text_field, text_field->cursor_y, text_field->cursor_x++) = c;
}

void TextField_PutString(TextField *text_field, const char *string)
{
    for (const char *c = string; *c; ++c) {
        TextField_PutChar(text_field, *c);
    }
}

void TextField_PutLine(TextField *text_field, const char *line)
{
    TextField_PutString(text_field, line);
    TextField_PutChar(text_field, '\n');
}

void TextField_Printf(TextField *text_field, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    int length = vsnprintf(
        TextField_CharAt(
            text_field, text_field->cursor_y, text_field->cursor_x),
        text_field->width - text_field->cursor_x,
        fmt, args
    );

    char *end = TextField_CharAt(
        text_field, text_field->cursor_y, text_field->cursor_x + length - 1);
    if (*end == '\n') {
        *end = ' ';
        TextField_Scroll(text_field);
        TextField_Flush(text_field);
    } else {
        text_field->cursor_x = UintMin(
            text_field->cursor_x + length, text_field->width);
    }
}

void TextField_ShowCursor(TextField *text_field)
{
    text_field->show_cursor = true;
}

void TextField_HideCursor(TextField *text_field)
{
    text_field->show_cursor = false;
}

void TextField_SetCursor(TextField *text_field, uint8_t cursor)
{
    ASSERT(cursor < text_field->width);
    text_field->cursor_x = cursor;
}

void TextField_MoveCursor(TextField *text_field, int16_t delta)
{
    int16_t new_x = text_field->cursor_x + delta;

    if (new_x >= text_field->width) {
        new_x = text_field->width - 1;
    } else if (new_x < 0) {
        new_x = 0;
    }

    text_field->cursor_x = new_x;
}

uint8_t TextField_GetCursor(const TextField *text_field)
{
    return text_field->cursor_x;
}

uint8_t TextField_GetWidth(const TextField *text_field)
{
    return text_field->width;
}

void TextField_Flush(TextField *text_field)
{
    uint32_t num_vertices = 6*text_field->width*text_field->height;
        // Each character slot is a quadrilateral, or two triangles, so requires
        // 6 vertices.

    glBindVertexArray(text_field->vao);
    {
        // Create the texture coordinate buffer.
        glBindBuffer(GL_ARRAY_BUFFER, text_field->vertex_uv);
        {
            vec2 *uv = Malloc(num_vertices*sizeof(vec2));
            vec2 char_coords;

            uint16_t i = 0;
                // Offset of the current vertex in `uv`.
            for (uint8_t row = 0; row < text_field->height; ++row) {
                for (uint8_t col = 0; col < text_field->width; ++col) {
                    Font_Coords(
                        *TextField_CharAt(text_field, row, col), &char_coords);

                    // When we initialized the vertex buffer, we set up the
                    // vertex positions for a quadrilateral like this one:
                    //
                    //               (x, y)        (x + char_width, y)
                    //                     A-----,B
                    //                     | 1  / |
                    //                     |  ,`  |
                    //                     | /  2 |
                    //                     C------D
                    // (x, y - char_height)        (x + char_width, y - char_height)
                    //
                    // Now we have to initialize uv texture coordinates which
                    // correspond to these two triangles. Remember that
                    // char_coords represents the _bottom_ left corner of the
                    // character in texture space, so we will be adding to
                    // char_coords.y, not subtracting.

                    // Triangle 1: top left, ABC
                    uv[i++] = (vec2){ char_coords.x,            char_coords.y+FONT_HEIGHT };
                    uv[i++] = (vec2){ char_coords.x+FONT_WIDTH, char_coords.y+FONT_HEIGHT };
                    uv[i++] = (vec2){ char_coords.x,            char_coords.y };

                    // Triangle 2: bottom right, DBC
                    uv[i++] = (vec2){ char_coords.x+FONT_WIDTH, char_coords.y };
                    uv[i++] = (vec2){ char_coords.x+FONT_WIDTH, char_coords.y+FONT_HEIGHT };
                    uv[i++] = (vec2){ char_coords.x,            char_coords.y };
                }
            }

            glBufferData(
                GL_ARRAY_BUFFER,
                num_vertices*sizeof(vec2),
                uv,
                GL_STATIC_DRAW
            );
            glVertexAttribPointer(
                VERTEX_ATTRIB_TEXTURE_UV, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(VERTEX_ATTRIB_TEXTURE_UV);

            free(uv);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Create the cursor toggle buffer.
        glBindBuffer(GL_ARRAY_BUFFER, text_field->vertex_cursor);
        {
            GLuint *cursors = Malloc(num_vertices*sizeof(GLuint));

            uint16_t i = 0;
                // Offset of the current vertex in `cursors`.
            for (uint8_t row = 0; row < text_field->height; ++row) {
                for (uint8_t col = 0; col < text_field->width; ++col) {
                    // Initialize cursors for the following quadrilateral:
                    //
                    //               (x, y)        (x + char_width, y)
                    //                     A-----,B
                    //                     | 1  / |
                    //                     |  ,`  |
                    //                     | /  2 |
                    //                     C------D
                    // (x, y - char_height)        (x + char_width, y - char_height)
                    //
                    // All four vertices in the quadrilateral will have the same
                    // value, since the cursor always occupies a whole cell.
                    // Most quads will have a value of `false`. A quadrilateral
                    // _is_ a cursor if and only if `text_field->show_cursor` is
                    // set and `x == text_field->cursor_x` and
                    // `y == text_field->cursor_y`.
                    bool cursor = text_field->show_cursor &&
                                  col == text_field->cursor_x &&
                                  row == text_field->cursor_y;

                    // Triangle 1: top left, ABC
                    cursors[i++] = cursor;
                    cursors[i++] = cursor;
                    cursors[i++] = cursor;

                    // Triangle 2: bottom right, DBC
                    cursors[i++] = cursor;
                    cursors[i++] = cursor;
                    cursors[i++] = cursor;
                }
            }

            glBufferData(
                GL_ARRAY_BUFFER,
                num_vertices*sizeof(GLuint),
                cursors,
                GL_STATIC_DRAW
            );
            glVertexAttribIPointer(
                VERTEX_ATTRIB_CURSOR, 1, GL_UNSIGNED_INT, 0, 0);
            glEnableVertexAttribArray(VERTEX_ATTRIB_CURSOR);

            free(cursors);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    glBindVertexArray(0);
}

