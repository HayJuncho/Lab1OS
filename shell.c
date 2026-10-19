#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#define MAX_COMMAND_LINE_LEN  1024
#define MAX_COMMAND_LINE_ARGS 128
#define MAX_PIPELINE          16   /* max commands joined by '|' */
#define TIMEOUT_SECONDS       10   /* foreground processes are killed after this */

extern char **environ;

static const char delimiters[] = " \t\r\n";

/* One stage of a pipeline: its arguments plus optional < and > files. */
typedef struct {
    char *argv[MAX_COMMAND_LINE_ARGS];
    char *infile;    /* file after '<', or NULL */
    char *outfile;   /* file after '>', or NULL */
} Command;


/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */

/* Collect any finished background children so they don't become zombies. */
static void reap_background(void)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

/*
 * Print the prompt (current working directory + "> ") and read one
 * non-blank line into buf with the trailing newline removed.
 * Returns false on EOF (ctrl-D), true otherwise.
 */
static bool read_line(char *buf, size_t size)
{
    while (true) {
        reap_background();

        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            printf("%s> ", cwd);
        else
            printf("> ");
        fflush(stdout);

        if (fgets(buf, size, stdin) == NULL) {
            if (ferror(stdin)) {
                fprintf(stderr, "fgets error\n");
                exit(1);
            }
            printf("\n");           /* EOF: finish the prompt line */
            return false;
        }

        buf[strcspn(buf, "\n")] = '\0';

        /* Re-prompt on empty / whitespace-only lines. */
        if (buf[strspn(buf, delimiters)] != '\0')
            return true;
    }
}


/* ------------------------------------------------------------------ */
/* Parsing                                                             */
/* ------------------------------------------------------------------ */

/*
 * Split the line into tokens, expand $VARS, and build the pipeline.
 *   - a trailing "&" marks a background job
 *   - "|" separates pipeline stages
 *   - "<" file / ">" file set per-stage redirection
 * Operators must be separated from other words by whitespace.
 * Returns false (after printing a message) on a syntax error.
 * *ncmds is set to 0 if there is nothing to run.
 */
static bool parse_line(char *line, Command *cmds, int *ncmds, bool *background)
{
    char *tokens[MAX_COMMAND_LINE_ARGS];
    int nt = 0;

    for (char *tok = strtok(line, delimiters); tok != NULL;
         tok = strtok(NULL, delimiters)) {
        if (nt >= MAX_COMMAND_LINE_ARGS - 1) {
            fprintf(stderr, "Too many arguments\n");
            return false;
        }
        if (tok[0] == '$' && tok[1] != '\0') {
            char *value = getenv(tok + 1);
            tokens[nt++] = value ? value : "";
        } else {
            tokens[nt++] = tok;
        }
    }
    tokens[nt] = NULL;

    *background = false;
    if (nt > 0 && strcmp(tokens[nt - 1], "&") == 0) {
        *background = true;
        nt--;
    }

    int n = 0;      /* index of the command being built */
    int argc = 0;   /* args collected for that command  */
    memset(&cmds[0], 0, sizeof(Command));

    for (int i = 0; i < nt; i++) {
        char *t = tokens[i];

        if (strcmp(t, "|") == 0) {
            if (argc == 0) {
                fprintf(stderr, "syntax error: missing command before '|'\n");
                return false;
            }
            if (n + 1 >= MAX_PIPELINE) {
                fprintf(stderr, "Pipeline too long\n");
                return false;
            }
            cmds[n].argv[argc] = NULL;
            n++;
            memset(&cmds[n], 0, sizeof(Command));
            argc = 0;
        }
        else if (strcmp(t, "<") == 0 || strcmp(t, ">") == 0) {
            if (i + 1 >= nt || strcmp(tokens[i + 1], "|") == 0 ||
                strcmp(tokens[i + 1], "<") == 0 ||
                strcmp(tokens[i + 1], ">") == 0) {
                fprintf(stderr, "syntax error: missing file after '%s'\n", t);
                return false;
            }
            if (t[0] == '<')
                cmds[n].infile = tokens[++i];
            else
                cmds[n].outfile = tokens[++i];
        }
        else {
            cmds[n].argv[argc++] = t;
        }
    }
    cmds[n].argv[argc] = NULL;

    if (argc == 0) {
        if (nt == 0) {          /* e.g. a lone "&" */
            *ncmds = 0;
            return true;
        }
        fprintf(stderr, "syntax error: missing command\n");
        return false;
    }

    *ncmds = n + 1;
    return true;
}


/* ------------------------------------------------------------------ */
/* Built-in commands                                                   */
/* ------------------------------------------------------------------ */

static bool is_builtin(const char *name)
{
    static const char *names[] = { "exit", "pwd", "cd", "echo", "env", "setenv", NULL };
    for (int i = 0; names[i] != NULL; i++)
        if (strcmp(name, names[i]) == 0)
            return true;
    return false;
}

static void run_builtin(char **args)
{
    const char *cmd = args[0];

    if (strcmp(cmd, "exit") == 0) {
        exit(0);
    }
    else if (strcmp(cmd, "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            printf("%s\n", cwd);
        else
            perror("pwd");
    }
    else if (strcmp(cmd, "cd") == 0) {
        if (args[1] == NULL) {
            const char *home = getenv("HOME");
            if (home != NULL && chdir(home) != 0)
                perror("cd");
        } else if (chdir(args[1]) != 0) {
            perror("cd");
        }
    }
    else if (strcmp(cmd, "echo") == 0) {
        for (int i = 1; args[i] != NULL; i++)
            printf("%s%s", args[i], args[i + 1] ? " " : "");
        printf("\n");
    }
    else if (strcmp(cmd, "env") == 0) {
        if (args[1] == NULL) {
            for (int i = 0; environ[i] != NULL; i++)
                printf("%s\n", environ[i]);
        } else {
            char *value = getenv(args[1]);
            if (value != NULL)
                printf("%s\n", value);
        }
    }
    else if (strcmp(cmd, "setenv") == 0) {
        /* Supports both "setenv NAME VALUE" and "setenv NAME=VALUE". */
        if (args[1] == NULL) {
            fprintf(stderr, "usage: setenv NAME=VALUE | setenv NAME VALUE\n");
        } else if (args[2] != NULL) {
            if (setenv(args[1], args[2], 1) != 0)
                perror("setenv");
        } else {
            char *eq = strchr(args[1], '=');
            if (eq == NULL || eq == args[1]) {
                fprintf(stderr, "usage: setenv NAME=VALUE | setenv NAME VALUE\n");
            } else {
                char name[MAX_COMMAND_LINE_LEN];
                size_t len = (size_t)(eq - args[1]);
                if (len >= sizeof(name))
                    len = sizeof(name) - 1;
                memcpy(name, args[1], len);
                name[len] = '\0';
                if (setenv(name, eq + 1, 1) != 0)
                    perror("setenv");
            }
        }
    }
    fflush(stdout);
}


/* ------------------------------------------------------------------ */
/* Execution                                                           */
/* ------------------------------------------------------------------ */

/* Apply a command's < and > redirections to the current process. */
static bool apply_redirects(const Command *c)
{
    if (c->infile != NULL) {
        int fd = open(c->infile, O_RDONLY);
        if (fd < 0) {
            perror(c->infile);
            return false;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (c->outfile != NULL) {
        int fd = open(c->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror(c->outfile);
            return false;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    return true;
}

/*
 * Wait for the foreground children. Polls with a short sleep so fast
 * commands return immediately; anything still running after
 * TIMEOUT_SECONDS is killed.
 */
static void wait_foreground(pid_t *pids, int n)
{
    bool done[MAX_PIPELINE] = { false };
    int remaining = n;
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (remaining > 0) {
        for (int i = 0; i < n; i++) {
            if (done[i])
                continue;
            pid_t r = waitpid(pids[i], NULL, WNOHANG);
            if (r == pids[i] || r < 0) {
                done[i] = true;
                remaining--;
            }
        }
        if (remaining == 0)
            break;

        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (now.tv_sec - start.tv_sec) +
                         (now.tv_nsec - start.tv_nsec) / 1e9;
        if (elapsed >= TIMEOUT_SECONDS) {
            printf("Process timed out. Killing process.\n");
            for (int i = 0; i < n; i++) {
                if (!done[i]) {
                    kill(pids[i], SIGKILL);
                    waitpid(pids[i], NULL, 0);
                }
            }
            return;
        }
        struct timespec pause = { 0, 5 * 1000 * 1000 };   /* 5 ms */
        nanosleep(&pause, NULL);
    }
}

/* Run a pipeline of n commands (n == 1 for a plain command). */
static void execute_pipeline(Command *cmds, int n, bool background)
{
    /*
     * A lone built-in must run in the shell itself (otherwise cd/setenv/exit
     * would only affect a child). Temporarily apply any redirection around it.
     */
    if (n == 1 && is_builtin(cmds[0].argv[0])) {
        int saved_in  = dup(STDIN_FILENO);
        int saved_out = dup(STDOUT_FILENO);
        if (apply_redirects(&cmds[0]))
            run_builtin(cmds[0].argv);
        fflush(stdout);
        dup2(saved_in,  STDIN_FILENO);
        dup2(saved_out, STDOUT_FILENO);
        close(saved_in);
        close(saved_out);
        return;
    }

    pid_t pids[MAX_PIPELINE];
    int started = 0;
    int prev_read = -1;     /* read end of the previous stage's pipe */

    for (int i = 0; i < n; i++) {
        int fd[2] = { -1, -1 };

        if (i < n - 1 && pipe(fd) < 0) {
            perror("pipe");
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            if (fd[0] != -1) { close(fd[0]); close(fd[1]); }
            break;
        }

        if (pid == 0) {
            /* ---- child ---- */
            signal(SIGINT, SIG_DFL);   /* ctrl-C should kill the child */

            if (prev_read != -1) {
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);
            }
            if (fd[1] != -1) {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
            }
            /* Explicit < and > override the pipe ends. */
            if (!apply_redirects(&cmds[i]))
                exit(1);

            if (is_builtin(cmds[i].argv[0])) {
                run_builtin(cmds[i].argv);
                exit(0);
            }

            execvp(cmds[i].argv[0], cmds[i].argv);
            perror("execvp() failed");
            printf("An error occurred.\n");
            exit(1);
        }

        /* ---- parent ---- */
        pids[started++] = pid;
        if (prev_read != -1)
            close(prev_read);
        if (fd[1] != -1)
            close(fd[1]);
        prev_read = fd[0];
    }

    if (prev_read != -1)
        close(prev_read);

    if (!background)
        wait_foreground(pids, started);
}


/* ------------------------------------------------------------------ */
/* Main loop                                                           */
/* ------------------------------------------------------------------ */

int main(void)
{
    char command_line[MAX_COMMAND_LINE_LEN];
    Command cmds[MAX_PIPELINE];

    /* The shell itself ignores ctrl-C; children restore the default. */
    signal(SIGINT, SIG_IGN);

    while (read_line(command_line, sizeof(command_line))) {
        int ncmds = 0;
        bool background = false;

        if (!parse_line(command_line, cmds, &ncmds, &background))
            continue;
        if (ncmds == 0)
            continue;

        execute_pipeline(cmds, ncmds, background);
    }

    return 0;
}
