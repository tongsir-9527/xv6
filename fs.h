// 修改 NDIRECT 为 11，增加双间接块
#define NDIRECT 11
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT + NINDIRECT * NINDIRECT)

// 磁盘 inode 结构
struct dinode {
  short type;          // 文件类型
  short major;         // 主设备号
  short minor;         // 次设备号
  short nlink;         // 链接数
  uint size;           // 文件大小（字节）
  uint addrs[NDIRECT + 2];  // 直接块 (11) + 单间接 (1) + 双间接 (1)
};
