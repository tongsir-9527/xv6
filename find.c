#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

static char *exec_cmd = 0;
static char *exec_argv[MAXARG];
static int exec_argc = 0;

void find(char *path, char *target) {
    int fd;
    struct stat st;
    char buf[512], *p;
    struct dirent de;

    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    if (st.type != T_DIR) {
        char *base = 0;
        for (char *q = path; *q; q++) {
            if (*q == '/') base = q;
        }
        if (base == 0) base = path;
        else base++;

        if (strcmp(base, target) == 0) {
            if (exec_cmd == 0) {
                printf("%s\n", path);
            } else {
                int pid = fork();
                if (pid < 0) {
                    fprintf(2, "find: fork failed\n");
                    close(fd);
                    return;
                }
                if (pid == 0) {
                    char *argv[MAXARG + 2];
                    int i = 0;
                    argv[i++] = exec_cmd;          // 命令名作为第一个参数
                    for (int j = 0; j < exec_argc; j++) {
                        argv[i++] = exec_argv[j];
                    }
                    argv[i++] = path;
                    argv[i] = 0;
                    exec(exec_cmd, argv);
                    fprintf(2, "find: exec failed for %s\n", path);
                    exit(1);
                } else {
                    wait(0);
                }
            }
        }
        close(fd);
        return;
    }

    strcpy(buf, path);
    p = buf + strlen(buf);
    if (p[-1] != '/') *p++ = '/';

    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) continue;
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
            continue;

        char *q = p;
        char *s = de.name;
        while (*s) *q++ = *s++;
        *q = 0;

        find(buf, target);
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(2, "Usage: find <dir> <name> [-exec cmd [args...]]\n");
        exit(1);
    }

    if (argc >= 4 && strcmp(argv[3], "-exec") == 0) {
        if (argc < 5) {
            fprintf(2, "find: -exec requires a command\n");
            exit(1);
        }
        exec_cmd = argv[4];
        exec_argc = 0;
        for (int i = 5; i < argc; i++) {
            if (exec_argc >= MAXARG) {
                fprintf(2, "find: too many arguments for exec\n");
                exit(1);
            }
            exec_argv[exec_argc++] = argv[i];
        }
    }

    find(argv[1], argv[2]);
    exit(0);
}
