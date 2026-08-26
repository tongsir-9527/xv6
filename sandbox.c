#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(2, "Usage: sandbox mask path command [args...]\n");
        exit(1);
    }
    int mask = atoi(argv[1]);
    char *path = argv[2];
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "sandbox: fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        // 子进程调用 interpose 设置沙箱
        if (interpose(mask, path) < 0) {
            fprintf(2, "sandbox: interpose failed\n");
            exit(1);
        }
        // 执行命令
        exec(argv[3], &argv[3]);
        fprintf(2, "sandbox: exec failed\n");
        exit(1);
    } else {
        wait(0);
        exit(0);
    }
}
