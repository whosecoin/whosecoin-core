#ifndef CLI_H
#define CLI_H

#include <util/list.h>

/**
 * The cli_t struct is a command line interface component that works within a libuv
 * event loop to handle commands of the form: CMD ARG0 ARG1 ARG2 ... Callbacks
 */
typedef struct cli cli_t;

/**
 * The cli_handler type is a functional type that handles a single command. Each handler
 * takes two parameters, the context pointer set in cli_create and a list of arguments.
 */
typedef int (*cli_handler)(void *ctx, list_t *args);

/**
 * Create a command line interface with the given context data. The context data should
 * contain all information (other than the arguments) needed by the handler functions.
 */
cli_t* cli_create(void *ctx, void (*ctx_free)(void*));

/**
 * Register a new command with the cli system.
 * @param cmd the command string to look for.
 * @param usage a short description of how to use the command.
 * @param handler the handler function for the command.
 */
void cli_add_command(cli_t *self, char *cmd, char *usage, cli_handler handler);

/**
 * Destroy the cli subsystem and destroy all associated objects.
 * @param self the cli
 */
void cli_destroy(cli_t *self);

#endif /* CLI_H */