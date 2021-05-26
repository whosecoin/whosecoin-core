#include <cli.h>
#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

typedef struct cli {
    void *ctx;
    void (*ctx_free)(void*);
    list_t *command;
    list_t *usage;
    list_t *handler;
    uv_tty_t tty;
} cli_t;

static void alloc_read_buffer(uv_handle_t *handle, size_t size, uv_buf_t *buf) {
    char *base = (char *) calloc(1, size);
    if (!base) {
        *buf = uv_buf_init(NULL, 0);
    } else {
        *buf = uv_buf_init(base, size);
    }
}

static list_t* split(char *str) {
    list_t *res = list_create(4);
    const char *delim = " ";
    char *token = strtok(str, delim);
    while(token != NULL) {
        char *item = malloc(strlen(token) + 1);
        strcpy(item, token);
        list_add(res, item);
        token = strtok(NULL, delim);
    }
    return res;
}

static void print_usage(cli_t *self) {
    printf("Use the following commands: \n");
    for (size_t i = 0; i < list_size(self->command); i++) {
        char *cmd = list_get(self->command, i);
        char *usage = list_get(self->usage, i);
        printf("   %-12s%s\n", cmd, usage);
    }
}

static void read_stdin(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    cli_t *self = stream->data;
    bool handled = false;
    if (nread == -1) {
        fprintf(stderr, "error: cli: unable to read from stdin\n");
    } else if (nread > 0) {

        char command[nread];
        memcpy(command, buf->base, nread);
        command[nread - 1] = '\0';
        list_t *args = split(command);

        if (list_size(args) >= 1) {
            for (size_t i = 0; i < list_size(self->command); i += 1) {
                char *cmd = list_get(self->command, i);
                cli_handler handler = list_get(self->handler, i);
                if (strcmp(cmd, list_get(args, 0))  == 0) {
                    handled |= handler(self->ctx, args) == 0;
                }
            }
        }

        if (!handled) print_usage(self);
        printf(">>> ");
        fflush(stdout);
        list_destroy(args, free);
    }
}
cli_t* cli_create(void *ctx, void (*ctx_free)(void*)) {
    cli_t *self = malloc(sizeof(cli_t));
    self->command = list_create(4);
    self->usage = list_create(4);
    self->handler = list_create(4);
    self->ctx = ctx;
    self->ctx_free = ctx_free;

    uv_tty_init(uv_default_loop(), &self->tty, STDIN_FILENO, true);
    self->tty.data = self;

    uv_read_start((uv_stream_t*)&self->tty, alloc_read_buffer, read_stdin);
    printf(">>> ");
    fflush(stdout);
    return self;
}

void cli_add_command(cli_t *self, char *cmd, char *usage, cli_handler handler) {
    list_add(self->command, cmd);
    list_add(self->usage, usage);
    list_add(self->handler, handler);
}

void cli_destroy(cli_t *self) {
    if (self == NULL) return;
    list_destroy(self->command, NULL);
    list_destroy(self->usage, NULL);
    list_destroy(self->handler, NULL);
    if (self->ctx_free) self->ctx_free(self->ctx);
    free(self);
}
