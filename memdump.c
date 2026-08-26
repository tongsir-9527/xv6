#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 小端序读取函数
static unsigned short get16(unsigned char *p) {
    return (unsigned short)p[0] | ((unsigned short)p[1] << 8);
}
static unsigned int get32(unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}
static unsigned long long get64(unsigned char *p) {
    return (unsigned long long)get32(p) | 
           ((unsigned long long)get32(p+4) << 32);
}

void memdump(char *fmt, char *data) {
    unsigned char *p = (unsigned char *)data;
    for (int i = 0; fmt[i]; i++) {
        switch (fmt[i]) {
            case 'i':
                printf("%d\n", get32(p));
                p += 4;
                break;
            case 'p':
                printf("%llx\n", get64(p));
                p += 8;
                break;
            case 'h':
                printf("%d\n", get16(p));
                p += 2;
                break;
            case 'c':
                printf("%c\n", *p);
                p += 1;
                break;
            case 's': {
                char *str = (char *)(uint64)get64(p);
                printf("%s\n", str);
                p += 8;
                break;
            }
            case 'S':
                printf("%s\n", (char *)p);
                while (*p) p++;
                p++; // skip '\0'
                break;
            default:
                // 忽略未知字符
                break;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        // 无参数：简单输出提示（可选）
        printf("Usage: memdump <format>\n");
        printf("Example: echo deadc0de | memdump hhcccc\n");
        exit(0);
    }

    // 读取标准输入所有数据
    char buf[4096];
    int n = 0;
    char ch;
    while (read(0, &ch, 1) > 0) {
        if (n < sizeof(buf)-1) buf[n++] = ch;
    }
    buf[n] = '\0';

    memdump(argv[1], buf);
    exit(0);
}
