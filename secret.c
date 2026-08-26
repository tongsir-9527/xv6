#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: secret <string>\n");
        exit(1);
    }
    // 分配一页内存，写入秘密字符串
    char *p = sbrk(4096);
    if (p == (char*)-1) {
        fprintf(2, "secret: sbrk failed\n");
        exit(1);
    }
    // 复制秘密字符串到分配的内存
    for (int i = 0; argv[1][i]; i++) {
        p[i] = argv[1][i];
    }
    p[strlen(argv[1])] = 0;  // 确保以 NULL 结尾
    // 这里不打印任何东西，只是分配内存后退出
    exit(0);
}
