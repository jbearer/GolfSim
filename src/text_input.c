#include "errors.h"
#include "text.h"

// Get the character in the input buffer `offset` columns from the current input
// position.
static char TextInput_RelativeChar(TextInput *text_input, int16_t offset)
{
    uint8_t x = TextField_GetCursor((TextField *)text_input);
    uint8_t prompt_len = strlen(text_input->prompt);

    ASSERT((int16_t)x + offset >= (int16_t)prompt_len);
    ASSERT((int16_t)x + offset < (int16_t)prompt_len + text_input->num_buffered);
    return text_input->buffer[x + offset - prompt_len];
}

// Get the character at the current input position in the input buffer.
static char TextInput_GetChar(TextInput *text_input)
{
    return TextInput_RelativeChar(text_input, 0);
}

// Delete the character under the cursor. Characters after the cursor on the
// same line will be shifted back by one.
static void TextInput_Delete(TextInput *text_input)
{
    uint8_t x = TextField_GetCursor((TextField *)text_input);
    uint8_t w = TextField_GetWidth((TextField *)text_input);
    uint8_t prompt_len = strlen(text_input->prompt);

    if (x - prompt_len >= text_input->num_buffered) {
        // Cursor is past the end of buffered input, so there is nothing to
        // delete.
        return;
    }

    ASSERT(text_input->num_buffered <= w);
    ASSERT(x < w);
    ASSERT(x >= prompt_len);

    char *c = &text_input->buffer[x - prompt_len];
        // Get a pointer into the input buffer where we're going to be deleting.

    // Update the input buffer by shifting all characters after the cursor by 1.
    memmove(c, c + 1, text_input->num_buffered - (x - prompt_len) - 1);
    --text_input->num_buffered;

    // Update the output buffer.
    TextField_SetCursor((TextField *)text_input, prompt_len);
        // Move the cursor to the beginning of the line.
    for (uint8_t i = 0; i < text_input->num_buffered; ++i) {
        // Write the contents of the updated input buffer into the line.
        TextField_PutChar((TextField *)text_input, text_input->buffer[i]);
    }
    TextField_PutChar((TextField *)text_input, ' ');
        // Write a blank character over the former last character in the line.

    // Move the cursor back to the delete position.
    TextField_SetCursor((TextField *)text_input, x);

    // We didn't write a newline, so we have to flush the text field.
    TextField_Flush((TextField *)text_input);
}

// Remove the character before the cursor. Characters at the cursor and after it
// on the current line are shifted back by one.
static void TextInput_Backspace(TextInput *text_input)
{
    uint8_t prompt_len = strlen(text_input->prompt);

    if (TextField_GetCursor((TextField *)text_input) <= prompt_len) {
        // The cursor is at the beginning of the line, so there is nothing
        // before it to remove.
        return;
    }

    // Move the cursor over the character we're going to remove, and delete it.
    TextField_MoveCursor((TextField *)text_input, -1);
    TextInput_Delete(text_input);
}

// Handle a key event. This includes handling special control characters, as
// well as general ASCII input and line processing.
static void TextInput_HandleKey(
    View *view, int key, KeyAction action, ModifierKey mods)
{
    TextInput *text_input = (TextInput *)view;
    uint8_t prompt_len = strlen(text_input->prompt);

    trace("Text input %#lx got key event %s %#x\n",
        (unsigned long)text_input,
        action == KEY_PRESS   ? "PRESS" :
        action == KEY_REPEAT  ? "REPEAT" :
        action == KEY_RELEASE ? "RELEASE" :
                                "UNKNOWN",
        key);

    if (action == KEY_RELEASE) {
        return;
    }

    switch (key) {
        case GLFW_KEY_BACKSPACE:
            do {
                TextInput_Backspace(text_input);
            } while (
                (mods & MOD_CONTROL) &&
                    // If control is pressed, keep backspacing until...
                TextField_GetCursor((TextField *)text_input) > prompt_len &&
                    // ...we hit the beginning of the line, or...
                TextInput_RelativeChar(text_input, -1) != ' '
                    // ...we hit the beginning of a word.
            );
            break;
        case GLFW_KEY_DELETE:
            do {
                TextInput_Delete(text_input);
            } while (
                (mods & MOD_CONTROL) &&
                    // If control is pressed, keep deleting until...
                TextField_GetCursor((TextField *)text_input)
                    < prompt_len + text_input->num_buffered &&
                    // ...we hit the end of the line, or...
                TextInput_GetChar(text_input) != ' '
                    // ...we hit the end of a word.
            );
            break;
        case GLFW_KEY_ENTER:
            TextField_PutChar((TextField *)text_input, '\n');
            trace(
                "Text input %#lx got line %.*s\n",
                (unsigned long)text_input,
                text_input->num_buffered,
                text_input->buffer
            );

            text_input->buffer[text_input->num_buffered] = '\0';
                // Terminate the input buffer.
            text_input->handle_line(text_input, text_input->buffer);
                // Handle a line of input.
            text_input->num_buffered = 0;
                // Clear the input buffer in preparation for the next line.

            // Print the prompt before the next line.
            TextField_PutString((TextField *)text_input, text_input->prompt);
            TextField_Flush((TextField *)text_input);
            break;
        case GLFW_KEY_LEFT:
            do {
                TextField_MoveCursor((TextField *)text_input, -1);
            } while (
                (mods & MOD_CONTROL) &&
                    // If control is pressed, keep moving until...
                TextField_GetCursor((TextField *)text_input) > prompt_len &&
                    // ...we hit the beginning of the line, or...
                TextInput_RelativeChar(text_input, -1) != ' '
                    // ...we hit the beginning of a word.
            );
            break;
        case GLFW_KEY_RIGHT:
            do {
                TextField_MoveCursor((TextField *)text_input, 1);
            } while (
                (mods & MOD_CONTROL) &&
                    // If control is pressed, keep moving until...
                TextField_GetCursor((TextField *)text_input)
                    < prompt_len + text_input->num_buffered &&
                    // ...we hit the end of the line, or...
                TextInput_GetChar(text_input) != ' '
                    // ...we hit the end of a word.
            );
            break;
        case GLFW_KEY_ESCAPE:
            View_Close((View *)text_input);
            return;
        default:
            break;
    }

    TextField_Flush((TextField *)text_input);
}

static void TextInput_HandleCharacter(View *view, uint32_t key)
{
    TextInput *text_input = (TextInput *)view;
    uint8_t prompt_len = strlen(text_input->prompt);

    trace("Text input %#lx got character input %#x\n",
        (unsigned long)text_input, (unsigned)key);

    if (key > 0xffff) {
        // Codepoint is outside the range of printable Unicode codepoints. This
        // means it is probably a control character, so we won't even print a
        // placeholder.
        return;
    } else if (key > 127) {
        // Non-ASCII character, we won't even try to print it. We will print a
        // placeholder, though, since this is supposed to be a printable
        // character, and we want to indicate to the user that we received the
        // character, we just don't know how to print it.
        key = '?';
    } else {
        trace("Key %#x is ASCII character %c\n", (unsigned)key, (char)key);
    }

    uint8_t x = TextField_GetCursor((TextField *)text_input);

    // Add the charaacter to the input buffer.
    text_input->buffer[x - prompt_len] = key;
    if (x - prompt_len >= text_input->num_buffered) {
        text_input->num_buffered = x - prompt_len + 1;
    }

    // Echo the character to the text output.
    TextField_PutChar((TextField *)text_input, key);
    TextField_Flush((TextField *)text_input);
}

static void TextInput_Destroy(View *view)
{
    TextInput *text_input = (TextInput *)view;
    free(text_input->buffer);
    text_input->super_destroy((View *)text_input);
}

TextInput *TextInput_New(
    size_t size, ViewManager *manager, View *parent, uint16_t x, uint16_t y,
    uint8_t width, uint8_t height, uint8_t font_size,
    TextInputCallback handle_line)
{
    ASSERT(size >= sizeof(TextInput));
    TextInput *text_input = (TextInput *)TextField_New(
        size, manager, parent, x, y, width, height, font_size);
    TextField_ShowCursor((TextField *)text_input);
    text_input->handle_line = handle_line;
    text_input->buffer = Malloc(width);
    text_input->prompt = "";
    text_input->num_buffered = 0;

    // Set up a destructor to free the input buffer when the view is closed.
    text_input->super_destroy = View_SetDestroyCallback(
        (View *)text_input, TextInput_Destroy);

    // Set up window callbacks so we can respond to keyboard input.
    View_SetKeyCallback((View *)text_input, TextInput_HandleKey);
    View_SetCharacterCallback((View *)text_input, TextInput_HandleCharacter);

    return text_input;
}

void TextInput_SetPrompt(TextInput *text_input, const char *prompt)
{
    text_input->prompt = prompt;
    TextField_PutString((TextField *)text_input, prompt);
    TextField_Flush((TextField *)text_input);
        // We have to flush, since the prompt might not end in a newline.
}
