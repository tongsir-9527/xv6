#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void process_file(char *filename) {
    int fd = open(filename, 0);
    if (fd < 0) {
        fprintf(2, "sixfive: cannot open %s\n", filename);
        return;
    }

    char buf[1024];
    int n;
    int num = 0;
    int in_num = 0;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) {
            char c = buf[i];
            if (c >= '0' && c <= '9') {
                num = num * 10 + (c - '0');
                in_num = 1;
            } else {
                if (in_num) {
                    if (num % 5 == 0 || num % 6 == 0) {
                        printf("%d\n", num);
                    }
                    num = 0;
                    in_num = 0;
                }
            }
        }
    }

    if (in_num) {
        if (num % 5 == 0 || num % 6 == 0) {
            printf("%d\n", num);
        }
    }

    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: sixfive <files...>\n");
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        process_file(argv[i]);
    }
    exit(0);
}
