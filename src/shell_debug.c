#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/stat.h>

#define COMMAND_DIR "svn-shell-commands"
#define NOLOGIN_COMMAND COMMAND_DIR "/no-interactive-login"

int print_env(char **environ)
{
    FILE *fp = fopen("/tmp/svn.log", "a");
    if(!fp) return -1;
    for(char **env = environ; *env != 0; env++)
    {
        fwrite(*env, strlen((const char*)*env), 1, fp);
    }
    fwrite("\n", 1, 1, fp);
    fclose(fp);
    return 0;
}

int print_int(int log)
{
    FILE *fp = fopen("/tmp/svn.log", "a");
    if(!fp) return -1;
    char buf[1024];
    sprintf(buf, "%d\n", log);
    printf(buf);
    fwrite(buf, strlen(buf), 1, fp);
    memset(buf, 0, 1024);
    fwrite("\n", 1, 1, fp);
    fclose(fp);
    return 0;
}

int print_log(const char*name, const char *log)
{
    FILE *fp = fopen("/tmp/svn.log", "a");
    if(!fp) return -1;
    char buf[1024];
    sprintf(buf, "%s: <<%s>>\n", name, log);
    printf(buf);
    fwrite(buf, strlen(buf), 1, fp);
    memset(buf, 0, 1024);
    fwrite("\n", 1, 1, fp);
    fclose(fp);
    return 0;
}

int print_log_list(int argc, const char **argv)
{
    FILE *fp = fopen("/tmp/svn.log", "a");
    if(!fp) return -1;
    char buf[1024];
    for (size_t i = 0; i < argc; i++)
    {
        sprintf(buf, "%ld: <<%s>>\n", i, argv[i]);
        printf(buf);
        fwrite(buf, strlen(buf), 1, fp);
        memset(buf, 0, 1024);
    }
    fwrite("\n", 1, 1, fp);
    fclose(fp);
    return 0;
}

__attribute__((format(printf, 1, 2))) static void die(const char *err, ...)
{
    char msg[4096];
    va_list params;
    va_start(params, err);
    vsnprintf(msg, sizeof(msg), err, params);
    fprintf(stderr, "%s\n", msg);
    va_end(params);
    exit(1);
}

void *grow_alloc(void *ptr, int *cur, int new)
{
    if (*cur < new)
    {
        if (((*cur + 16) * 3 / 2) < new)
        {
            *cur = new;
        }
        else
        {
            *cur = (*cur + 16) * 3 / 2;
            void *ret = realloc(ptr, *cur);
            if (!ret)
            {
                free(ptr);
                return NULL;
            }
            return ret;
        }
    }
    return ptr;
}

#define ALLOC_FAILED 3
#define REALLOC_FAILED 4
#define SPLIT_CMDLINE_BAD_ENDING 1
#define SPLIT_CMDLINE_UNCLOSED_QUOTE 2
static const char *split_cmdline_errors[] = {
    ("cmdline ends with \\"),
    ("unclosed quote")};

int split_cmdline(char *cmdline, const char ***argv, int *count, int *size)
{
    int src, dst;
    char quoted = 0;
    if ((*count) == 0)
    {
        *argv = malloc((*size) * sizeof(*(*argv)));
        if(!(*argv))
        {
            return -ALLOC_FAILED;
        }
    }
    /* split alias_string */
    (*argv)[(*count)++] = cmdline;
    for (src = dst = 0; cmdline[src];)
    {
        char c = cmdline[src];
        if (!quoted && isspace(c))
        {
            cmdline[dst++] = 0;
            /* skip */
            while (cmdline[++src] && isspace(cmdline[src]))
            {
                ;
            }
            // realloc
            *argv = grow_alloc(*argv, size, (*count) + 1);
            if(!(*argv))
            {
                return -REALLOC_FAILED;
            }
            (*argv)[(*count)++] = cmdline + dst;
        }
        else if (!quoted && (c == '\'' || c == '"'))
        {
            quoted = c;
            src++;
        }
        else if (c == quoted)
        {
            quoted = 0;
            src++;
        }
        else
        {
            if (c == '\\' && quoted != '\'')
            {
                src++;
                c = cmdline[src];
                if (!c)
                {
                    free(*argv);
                    *argv = NULL;
                    return -SPLIT_CMDLINE_BAD_ENDING;
                }
            }
            cmdline[dst++] = c;
            src++;
        }
    }
    cmdline[dst] = 0;
    if (quoted)
    {
        free(*argv);
        *argv = NULL;
        return -SPLIT_CMDLINE_UNCLOSED_QUOTE;
    }
    // the last is empty string
    if (strlen((*argv)[(*count) - 1]) == 0)
    {
        // tail empty string
        (*argv)[(*count) - 1] = NULL;
        (*count)--;
    }
    else
    {
        // realloc NULL
        *argv = grow_alloc(*argv, size, (*count) + 1);
        if(!(*argv))
        {
            return -REALLOC_FAILED;
        }
        (*argv)[(*count)] = NULL;
    }
    return 0;
}

int read_default_option(char **buf, const char ***argv, int *count, int *size)
{
    struct stat meta;
    int ret = stat("/etc/default/svnserve", &meta);
    if(ret == -1) 
    {
        return 0;
    }
    *buf = malloc(meta.st_size + 1);
    if (!(*buf))
    {
        return -ALLOC_FAILED;
    }
    FILE *fp = fopen("/etc/default/svnserve", "r");
    if (!fp)
    {
        return 0;
    }
    fread((*buf), meta.st_size, 1, fp);
    fclose(fp);
    // parse
    (*buf)[meta.st_size] = 0;
    for (int i = 0, l = 0; i < meta.st_size + 1; i++)
    {
        switch ((*buf)[i])
        {
        case '\0':
        case '\n':
            // cut
            (*buf)[i] = 0;
            char *ptr = (*buf) + l;
            l = i + 1;
            // empty line
            if (strlen(ptr) == 0)
            {
                break;
            }
            // trim
            while (isspace(ptr[0]))
            {
                ptr++;
            }
            // comment
            if (ptr[0] == '#')
            {
                break;
            }
            int ret = split_cmdline(ptr, argv, count, size);
            if (ret)
            {
                free((*buf));
                return ret;
            }
            break;
        default:
            break;
        }
    }
    return 0;
}

int add_default_root_directory(const char ***argv, int *count, int *size)
{
    int root = 1;
    for (int i = 0; i < (*count); i++)
    {
        if (!strcmp((*argv)[i], "-r") || !strcmp((*argv)[i], "--root"))
        {
            root = 0;
        }
    }
    // add root directory if default option not exists
    if (root)
    {
        *argv = grow_alloc(*argv, size, (*count) + 2);
        if (!(*argv))
        {
            return -REALLOC_FAILED;
        }
        (*argv)[(*count)] = "-r";
        (*argv)[(*count) + 1] = getenv("HOME");
        (*argv)[(*count) + 2] = NULL;
        (*count) += 2;
    }
    return 0;
}

static int is_valid_cmd_name(const char *cmd)
{
    /* Test command contains no . or / characters */
    return cmd[strcspn(cmd, "./")] == '\0';
}

static char *make_cmd(const char *program)
{
    int size = strlen(COMMAND_DIR) + strlen(program);
    char *buf = malloc(size + 1);
    memset(buf, 0, size);
    sprintf(buf, "%s/%s", COMMAND_DIR, program);
    return buf;
}

static void cd_to_homedir(void)
{
    const char *home = getenv("HOME");
    if (!home)
        die("could not determine user's home directory; HOME is unset");
    if (chdir(home) == -1)
        die("could not chdir to user's home directory");
}

static void run_shell(void)
{
    if (!access(NOLOGIN_COMMAND, F_OK))
    {
        /* Interactive login disabled. */
        char *argv[] = {"/bin/sh", NOLOGIN_COMMAND, NULL};
        int status = execv("/bin/sh", argv);
        if (status < 0)
            exit(127);
        exit(status);
    }
}

int main(int argc, const char **argv)
{
    if (argc == 1)
    {
        /* Block the user to run an interactive shell */
        cd_to_homedir();
        if (access(COMMAND_DIR, R_OK | X_OK) == -1)
        {
            die("Interactive svn shell is not avaliable.\n"
                "hint: ~/" COMMAND_DIR " should exist "
                "and have read and execute access.");
        }
        run_shell();
        exit(0);
    }
    else if (argc != 3 || strcmp(argv[1], "-c"))
    {
        /*
         * We do not accept any other modes except "-c" followed by
         * "cmd arg", where "cmd" is a very limited subset of svn
         * commands or a command in the COMMAND_DIR
         */
        die("Run with no arguments or with -c cmd");
    }
    int count = 0, size = 16;
    const char **user_argv;
    char *program = strdup(argv[2]);
    if (!strncmp(program, "svnserve", 8) && isspace(program[8]) && strlen(program) > 9)
    {
        if (!strncmp(program + 9, "-t", 2))
        {
            int ret = split_cmdline(program, &user_argv, &count, &size);
            if (ret)
            {
                free(program);
                if (ret == SPLIT_CMDLINE_UNCLOSED_QUOTE || ret == SPLIT_CMDLINE_BAD_ENDING)
                {
                    die("Invalid command format '%s': %s", argv[2], split_cmdline_errors[-count - 1]);
                }
                else
                {
                    die("Out of memory, alloc failed");
                }
            }
            // check argument, can replace with getopt/getopt_long
            for (int i = 0; i < count; i++)
            {
                if (!strcmp(user_argv[i], "-r") || !strcmp(user_argv[i], "--root"))
                {
                    free(program);
                    free(user_argv);
                    if (i + 1 < count)
                    {
                        die("Root directory argument %s with %s cmd not allowed", user_argv[i + 1], user_argv[i]);
                    }
                    die("Root directory %s cmd not allowed", user_argv[i]);
                }
            }
            // addon svnserve options
            char *buf;
            ret = read_default_option(&buf, &user_argv, &count, &size);
            if(ret)
            {
                free(program);
                if (ret == SPLIT_CMDLINE_UNCLOSED_QUOTE || ret == SPLIT_CMDLINE_BAD_ENDING)
                {
                    die("Invalid command format '%s': %s", argv[2], split_cmdline_errors[-count - 1]);
                }
                else
                {
                    die("Out of memory, alloc failed");
                }
            }
            ret = add_default_root_directory(&user_argv, &count, &size);
            if(ret)
            {
                free(program);
                if (ret == SPLIT_CMDLINE_UNCLOSED_QUOTE || ret == SPLIT_CMDLINE_BAD_ENDING)
                {
                    die("Invalid command format '%s': %s", argv[2], split_cmdline_errors[-count - 1]);
                }
                else
                {
                    die("Out of memory, alloc failed");
                }
            }
            // disable for debug
            // ret = execvp(user_argv[0], (char *const *)user_argv);
            free(buf);
            free(program);
            free(user_argv);
            return ret;
        }
    }
    // if commands not match, run COMMAND_DIR commands
    cd_to_homedir();
    int ret = split_cmdline(program, &user_argv, &count, &size);
    if (!ret)
    {
        if (is_valid_cmd_name(user_argv[0]))
        {
            user_argv[0] = program = make_cmd(user_argv[0]);
            execv(user_argv[0], (char *const *)user_argv);
        }
        free(program);
        free(user_argv);
        die("Unrecognized command '%s'", argv[2]);
    }
    else
    {
        free(program);
        die("Invalid command format '%s': %s", argv[2], split_cmdline_errors[-count - 1]);
    }
}
