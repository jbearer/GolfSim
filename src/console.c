#include <assert.h>
#include <string.h>

#include "text.h"

#if !(_SVID_SOURCE || _BSD_SOURCE || _POSIX_C_SOURCE >= 1 || _XOPEN_SOURCE || _POSIX_SOURCE)
    // strtok_r not defined in string.h

char *strtok_r(char *str, const char *delim, char **saveptr)
{
    // Invariant: `saveptr` points to the first character of the next token to
    // return, or '\0' if there are no more tokens.

    if (str != NULL) {
        // First call. Consume initial delimiter characters.
        while (*str && strchr(delim, *str) != NULL) {
            ++str;
        }

        // Establish invariant by pointing `saveptr` at the first non-delimiter
        // character, which will become the start of the first token.
        *saveptr = str;
    }

    if (**saveptr == '\0') {
        return NULL;
    }

    char *tok = *saveptr;
        // Save the start of the token we're going to return. The rest of the
        // function will update `saveptr` to point to the start of the _next_
        // token.

    // We can't return an empty token or a delimiter character.
    assert(*tok != '\0');
    assert(strchr(delim, *tok) == NULL);

    // Consume non-delimiter bytes to find the end of this token.
    while (**saveptr && strchr(delim, **saveptr) == NULL) {
        ++*saveptr;
    }

    if (**saveptr) {
        assert(strchr(delim, **saveptr) != NULL);

        **saveptr = '\0';
            // Write '\0' into the place of the first delimiter character to
            // terminate the token we're going to return.
        ++*saveptr;
            // Advance `saveptr` past the end of the current token.

        // Consume delimiter characters until `saveptr` points to the start of
        // the next token or the end of the string.
        while (**saveptr && strchr(delim, **saveptr) != NULL) {
            ++*saveptr;
        }
    } else {
        // Nothing to do. `saveptr` points to the end of the string, and on the
        // next call we will observe this and return NULL.
    }

    return tok;
}

#endif // strtok_r

// Retrieve a pointer to the sub-command of `command` named by `name`, or NULL
// if there is no such sub-command.
static const Command *Console_FindSubCommand(
    const Command *command, const char *name)
{
    for (uint8_t i = 0; i < command->impl.sub_commands.num_commands; ++i) {
        Command *sub_command = command->impl.sub_commands.commands[i];
        sub_command->parent = command;
            // Lazily update the parent pointer of the sub-command.

        if (strcmp(sub_command->name, name) == 0) {
            return sub_command;
        }
    }

    return NULL;
}

// Write the formatted name of `command` (including all of its parent commands)
// to the given Console.
static void Console_PutCommandName(Console *console, const Command *command)
{
    if (command->parent != NULL && command->parent->name != NULL) {
        // If the command has a parent which is not the root command of the
        // program, recursively write the names of the parents.
        Console_PutCommandName(console, command->parent);
        TextField_PutChar((TextField *)console, ' ');
    }
    if (command->name != NULL) {
        // If the command is not the root command (which has no name) write the
        // name.
        TextField_PutString((TextField *)console, command->name);
    }
}

// Process a command input from the console.
static void Console_HandleLine(TextInput *text_input, char *line)
{
    Console *console = (Console *)text_input;
    bool help = false;
        // Will be set to true if we are asking for help on the named command,
        // rather than trying to execute that command.
    char *saveptr = NULL;
        // Internal state for `strtok_r`.
    const Command *command = console->program;
        // Root command of the program.
    assert(command->type == SUB_COMMANDS);

    const char *argv[TextField_GetWidth((TextField *)console) / 2];
        // Arguments are whitespace separated, so as a conservatie approximation
        // we cannot have more than one argument per two columns in the console
        // (one-character arguments separated by one character of whitespace).

    // Break `line` into tokens by splitting on whitespace.
    static const char *WS = " \t\n";
    char *tok = strtok_r(line, WS, &saveptr);
    if (tok == NULL) {
        // Empty command, do nothing.
        return;
    }

    // Parse first token: either "help" or a top-level command.
    if (strcmp("help", tok) == 0) {
        // Asking for help, either for the whole program or for a subcommand.
        help = true;
    } else {
        // tok is not "help", meaning it must be the first part of a command.
        assert(command->type == SUB_COMMANDS);
            // We're still at the top-level program, so we must have at least
            // one level of sub-commands.

        command = Console_FindSubCommand(command, tok);
        if (command == NULL) {
            TextField_PutLine((TextField *)console, "Unrecognized command.");
            return;
        }
    }

    // Parse optional sub-command tokens.
    while (command->type == SUB_COMMANDS &&
           (tok = strtok_r(NULL, WS, &saveptr)) != NULL) {

        command = Console_FindSubCommand(command, tok);
        if (command == NULL) {
            TextField_PutLine((TextField *)console, "Unrecognized command.");
            return;
        }

    }

    if (command->type != RUN_COMMAND) {
        // If this command is not terminal (that is, it requires sub-commands)
        // we can't run it, so we'll print a usage message instead.
        help = true;
    }

    if (help) {
        if (strtok_r(NULL, WS, &saveptr) != NULL) {
            // We've reached a terminal command (one with no more sub-commands)
            // but there are still tokens left in the input. Ordinarily, these
            // would be arguments to `command`. However, help doesn't take
            // arguments; it just takes the name of a command, so this is an
            // error.
            TextField_PutLine((TextField *)console, "help: no such command");
            return;
        }

        if (command != console->program) {
            assert(command->name != NULL);
            assert(command->help != NULL);

            // If this is not the unnamed root command, format the command's
            // name and help string.
            Console_PutCommandName(console, command);
            TextField_PutString((TextField *)console, " - ");
            TextField_PutLine((TextField *)console, command->help);
            TextField_PutLine((TextField *)console, "");
        }

        if (command->type == SUB_COMMANDS) {
            // Print a message informing the user that they can ask for help on
            // a sub-command of this command.
            TextField_PutString((TextField *)console, "Type 'help ");
            Console_PutCommandName(console, command);
            TextField_PutLine((TextField *)console,
                " <sub-command>' for help on a specific sub-command.");

            // Print the list of available sub-commands.
            TextField_PutLine((TextField *)console, "Available sub-commmands:");
            for (uint8_t i = 0;
                 i < command->impl.sub_commands.num_commands;
                 ++i) {

                const Command *sub_command =
                    command->impl.sub_commands.commands[i];
                TextField_PutString((TextField *)console, "  ");
                TextField_PutString((TextField *)console, sub_command->name);
                TextField_PutString((TextField *)console, " - ");
                TextField_PutLine((TextField *)console, sub_command->help);
            }
        }

        return;
    }

    // Tokenize the rest of the line. These tokens will become inputs (argv) to
    // the command.
    int argc = 0;
    while ((tok = strtok_r(NULL, WS, &saveptr)) != NULL) {
        argv[argc++] = tok;
    }

    // Run the command.
    assert(command->type == RUN_COMMAND);
    command->impl.run(console, console->state, argc, argv);
}

void Console_Init(Console *console, GLFWwindow *window,
    uint16_t x, uint16_t y, uint8_t width, uint8_t height, uint8_t font_size,
    const Command *program, void *state)
{
    TextInput_Init((TextInput *)console, window,
        x, y, width, height, font_size, Console_HandleLine);
    console->program = program;
    console->state = state;

    // Print a help message.
    TextField_PutLine((TextField *)console, "Press ESC to hide the console.");
    TextField_PutLine((TextField *)console, "Press Ctrl+Shift+P to show it again.");
    TextField_PutLine((TextField *)console, "Type 'help' for help.");

    // Print the prompt.
    TextInput_SetPrompt((TextInput *)console, "$ ");
}

void Console_Render(const Console *console)
{
    TextInput_Render((const TextInput *)console);
}
