/* ================= 头文件 ================= */
#include <stdio.h>   /* 标准输入输出: printf, scanf, fgets, putchar 等 */
#include <stdlib.h>  /* 通用工具: atoi, strtol, strtod 等字符串转数值函数 */
#include <string.h>  /* 字符串操作: strlen, memcpy 等 */
#include <stdint.h>  /* 固定宽度整数类型: uint32_t, uint64_t, int64_t 等 */
#include <ctype.h>   /* 字符分类: isspace, tolower 等 */
#include <errno.h>   /* 错误码: ERANGE (数值溢出时 strtoll 设置此值) */
#include <math.h>    /* 数学函数: isfinite, isinf, pow, floor, fabs (用于验证) */

/* ================= 通用工具函数 ================= */

/*
 * readLine: 从标准输入读取一整行文本
 * 参数: buf - 存储读取内容的字符数组, size - 数组最大容量
 * 返回: 成功读取返回 1, 遇到 EOF 返回 0
 * 说明: 自动去除行尾的换行符 \n 和回车符 \r (兼容 Windows/Linux)
 */
static int readLine(char *buf, size_t size)
{
    if (!fgets(buf, (int)size, stdin))    /* fgets 读取一行; 失败(EOF)则返回 0 */
        return 0;
    size_t len = strlen(buf);             /* 获取读入字符串的实际长度 */
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';                /* 从末尾逐个去除 \n 和 \r */
    return 1;                             /* 成功 */
}

/*
 * stripSpaces: 去除字符串中的所有空白字符(空格 /制表符等)
 * 参数: src - 原始字符串, dst - 输出缓冲区, dstSize - 缓冲区大小
 * 返回: 去除空白后的有效字符数
 * 用途: 让用户输入二进制时可以加空格分隔, 如 "1010 1100" -> "10101100"
 */
static int stripSpaces(const char *src, char *dst, size_t dstSize)
{
    size_t n = 0;                         /* 记录写入 dst 的字符数 */
    for (const char *p = src; *p && n + 1 < dstSize; p++) { /* 遍历源字符串每个字符 */
        if (!isspace((unsigned char)*p))  /* 如果不是空白字符 */
            dst[n++] = *p;                /* 复制到目标缓冲区 */
    }
    dst[n] = '\0';                        /* 添加字符串结束符 */
    return (int)n;                        /* 返回有效长度 */
}

/*
 * parseBinary: 将纯二进制字符串解析为 uint64 无符号整数
 * 参数: s - 仅含 '0'/'1' 的字符串, out - 输出解析结果
 * 返回: 成功返回 1, 字符串为空或含非法字符返回 0
 * 说明: 使用左移+或运算逐位累加, 如 "101" -> (1<<2)|(0<<1)|(1<<0) = 5
 */
static int parseBinary(const char *s, uint64_t *out)
{
    if (!*s)                              /* 空字符串, 解析失败 */
        return 0;
    uint64_t v = 0;                       /* 累加器, 初始为 0 */
    for (const char *p = s; *p; p++) {    /* 从左到右遍历每个字符 */
        if (*p != '0' && *p != '1')       /* 遇到非 0/1 字符 */
            return 0;                     /* 解析失败 */
        v = (v << 1) | (uint64_t)(*p - '0'); /* 左移一位腾出最低位, 或上当前位的值 */
    }
    *out = v;                             /* 将结果写入输出参数 */
    return 1;                             /* 成功 */
}

/*
 * parseHex: 将十六进制字符串解析为 uint64 无符号整数
 * 参数: s - 十六进制字符串 (允许 0x/0X 前缀, 大小写均可), out - 输出结果
 * 返回: 成功返回 1, 空串或含非法字符返回 0
 * 说明: 与 parseBinary 同风格, 逐字符左移累加; 每个hex字符贡献 4 个位
 */
static int parseHex(const char *s, uint64_t *out)
{
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) /* 跳过 0x/0X 前缀 */
        s += 2;
    if (!*s)                              /* 空字符串, 解析失败 */
        return 0;
    uint64_t v = 0;                       /* 累加器 */
    for (const char *p = s; *p; p++) {    /* 遍历每个字符 */
        char c = (char)tolower((unsigned char)*p); /* 统一转小写 */
        int d;                            /* 当前字符对应的数值 0~15 */
        if (c >= '0' && c <= '9')
            d = c - '0';                  /* '0'~'9' -> 0~9 */
        else if (c >= 'a' && c <= 'f')
            d = c - 'a' + 10;             /* 'a'~'f' -> 10~15 */
        else
            return 0;                     /* 非法字符, 解析失败 */
        v = (v << 4) | (uint64_t)d;       /* 左移 4 位腾出空间, 或上当前值 */
    }
    *out = v;                             /* 写入结果 */
    return 1;                             /* 成功 */
}

/*
 * toBinaryString: 将无符号整数转换为指定位宽的二进制字符串
 * 参数: val - 要转换的数值, bits - 目标位宽, out - 输出字符数组(需至少 bits+1 字节)
 * 说明: 从最高位开始逐位提取, 不足位宽时高位自动补 0
 * 例如 toBinaryString(5, 8, out) -> out = "00000101"
 */
static void toBinaryString(uint64_t val, int bits, char *out)
{
    for (int i = bits - 1; i >= 0; i--)   /* 从最高位(bits-1)遍历到最低位(0) */
        out[bits - 1 - i] = ((val >> i) & 1) ? '1' : '0'; /* 右移 i 位后取最低位 */
    out[bits] = '\0';                     /* 字符串结束符 */
}

/*
 * printGrouped: 按每 4 位一组打印二进制字符串, 组间加空格
 * 参数: s - 二进制字符串 (如 "00000101")
 * 输出: 格式化打印 (如 "0000 0101")
 * 说明: 从右往左数, 每 4 位插入一个空格, 便于阅读
 */
static void printGrouped(const char *s)
{
    int len = (int)strlen(s);             /* 获取字符串总长度 */
    for (int i = 0; i < len; i++) {       /* 逐字符遍历 */
        if (i > 0 && (len - i) % 4 == 0)  /* 如果当前位置右侧剩余字符数是 4 的倍数 */
            putchar(' ');                 /* 在当前字符前插入一个空格 */
        putchar(s[i]);                    /* 打印当前字符 */
    }
    putchar('\n');                        /* 末尾换行 */
}

/*
 * readLongLong: 从标准输入读取一个 long long 整数 (带严格校验)
 * 参数: out - 输出读取到的整数值
 * 返回: 成功返回 1, 输入非法返回 0
 * 说明: 使用 strtoll 解析, 会检测:
 *       1) 是否为空输入
 *       2) 是否数值溢出 (ERANGE)
 *       3) 末尾是否有多余的非法字符 (如 "123abc")
 */
static int readLongLong(long long *out)
{
    char line[256];                       /* 行缓冲区 */
    if (!readLine(line, sizeof line))     /* 读取一行 */
        return 0;
    errno = 0;                            /* 清除之前的错误码 */
    char *end = NULL;                     /* end 将指向第一个无法解析的字符 */
    long long v = strtoll(line, &end, 10);/* 以 10 进制解析字符串为 long long */
    if (end == line || errno == ERANGE)   /* end==line 表示没解析到任何数字; ERANGE 表示溢出 */
        return 0;
    while (*end && isspace((unsigned char)*end)) /* 跳过尾随空白 */
        end++;
    if (*end)                             /* 如果还有非空白字符残留 (如 "123abc") */
        return 0;                         /* 视为非法输入 */
    *out = v;                             /* 写入结果 */
    return 1;
}

/*
 * normalizeSciInput: 将 "9.625×10^3" 或 "9.625\times10^{-3}" 等
 *                    人类/LaTeX 写法统一改写为 "9.625e3", 便于 strtod 解析
 * 参数: s - 原始输入, dst - 改写结果缓冲区, dstSize - 缓冲区大小
 * 返回: 改写后的有效长度 (不识别的字符原样复制)
 */
static int normalizeSciInput(const char *src, char *dst, size_t dstSize)
{
    size_t n = 0;
    for (const char *p = src; *p && n + 1 < dstSize; p++) {
        if (strncmp(p, "\\times", 6) == 0) { /* LaTeX: \times -> e */
            dst[n++] = 'e';
            p += 5;                          /* 跳过 "\times" 剩余 5 个字符 */
        } else if (*p == '×' || *p == 'x' || *p == 'X') { /* 常见替换符号 -> e */
            /* 注意: 'x'/'X' 仅在其前后都是数字/符号时才算乘号, 此处简化处理 */
            dst[n++] = 'e';
        } else if (*p == '^') {              /* ^{-3} 或 ^3 -> 去掉 ^ 和花括号 */
            const char *q = p + 1;
            if (*q == '{') {                 /* ^{...}: 复制括号内内容 */
                q++;
                while (*q && *q != '}' && n + 1 < dstSize)
                    dst[n++] = *q++;
                if (*q == '}') p = q;        /* p 跳到 '}' 处, for 循环再 +1 */
            } else {
                dst[n++] = *p + 1 - 1;       /* ^ 后无花括号: 丢弃 '^' 本身 */
            }
        } else if (*p == '{' || *p == '}') { /* 单独的花括号丢弃 */
            continue;
        } else {
            dst[n++] = *p;                   /* 其他字符原样保留 */
        }
    }
    dst[n] = '\0';
    return (int)n;
}

/*
 * readDouble: 从标准输入读取一个 double 浮点数 (带严格校验)
 * 参数: out - 输出读取到的浮点值
 * 返回: 成功返回 1, 输入非法返回 0
 * 说明: 使用 strtod 解析, 支持小数点、科学计数法 (如 1.5e-3)、inf、nan
 *       解析前先用 normalizeSciInput 把 "×10^" / "\times" 等写法归一化为 e 写法
 *       校验逻辑与 readLongLong 相同: 空输入/无有效字符/尾随垃圾字符均拒绝
 */
static int readDouble(double *out)
{
    char line[256];                       /* 行缓冲区 */
    if (!readLine(line, sizeof line))     /* 读取一行 */
        return 0;
    char norm[256];                       /* 归一化后的字符串缓冲区 */
    normalizeSciInput(line, norm, sizeof norm); /* 先把 ×10^ 写法归一化为 e 写法 */
    char *end = NULL;                     /* 指向第一个无法解析的字符 */
    double v = strtod(norm, &end);        /* 解析字符串为 double */
    if (end == norm)                      /* 没解析到任何有效数字 */
        return 0;
    while (*end && isspace((unsigned char)*end)) /* 跳过尾随空白 */
        end++;
    if (*end)                             /* 有非空白垃圾字符残留 */
        return 0;
    *out = v;                             /* 写入结果 */
    return 1;
}

/* ================= 整数转换功能 ================= */

/*
 * chooseWidth: 让用户选择位宽 (8/16/32)
 * 返回: 8、16 或 32; 输入无效时循环提示; 遇到 EOF 返回 -1
 */
static int chooseWidth(void)
{
    char line[64];                        /* 行缓冲区 */
    while (1) {                           /* 循环直到输入合法 */
        printf("请选择位宽 (8/16/32): ");
        if (!readLine(line, sizeof line)) /* 读取一行, EOF 则退出 */
            return -1;
        char *end = NULL;
        long w = strtol(line, &end, 10);  /* 解析为 long */
        while (*end && isspace((unsigned char)*end)) /* 跳过尾随空白 */
            end++;
        if (*end == '\0' && (w == 8 || w == 16 || w == 32)) { /* 合法: 无残留且值为 8/16/32 */
            printf("已选择: %ld 位位宽\n", w);
            return (int)w;                /* 返回位宽 */
        }
        printf("输入无效, 请输入 8、16 或 32。\n"); /* 不合法则提示重新输入 */
    }
}

/*
 * printIntRep: 打印一个整数的补码, 以及负数时的原码和反码
 * 参数: value - 有符号十进制值, width - 位宽 (8/16/32)
 * 说明:
 *   - 补码: 负数用 2^n - |value|, 正数直接输出
 *   - 原码: 符号位 + 绝对值的二进制
 *   - 反码: 原码的数值位取反
 *   - 对于 -2^(n-1) (如 -128), 原码/反码无法表示, 给出特殊说明
 */
static void printIntRep(long long value, int width)
{
    uint64_t range = 1ULL << width;       /* 2^width, 即该位宽的总表示范围 */
    /* 计算补码的无符号表示: 负数用 2^n - |value|, 正数直接用原值 */
    uint64_t u = (value < 0) ? (range - (uint64_t)(-value)) : (uint64_t)value;
    char comp[64];                        /* 存放补码二进制字符串 */
    toBinaryString(u, width, comp);       /* 转换为二进制字符串 */
    printf("补码: ");
    printGrouped(comp);                   /* 分组打印补码 */

    if (value < 0) {                      /* 负数: 额外展示原码和反码 */
        uint64_t m = (uint64_t)(-value);  /* 绝对值 */
        uint64_t maxMag = (range >> 1) - 1; /* 原码/反码能表示的最大绝对值 = 2^(n-1) - 1 */
        if (m <= maxMag) {                /* 绝对值在原码/反码范围内 */
            char orig[64], inv[64];       /* 原码和反码的字符串缓冲区 */
            orig[0] = '1';                /* 原码符号位 = 1 (负数) */
            toBinaryString(m, width - 1, orig + 1); /* 原码数值位 = 绝对值的二进制 */
            inv[0] = '1';                 /* 反码符号位 = 1 (负数) */
            for (int i = 1; i < width; i++) /* 反码数值位 = 原码数值位逐位取反 */
                inv[i] = (orig[i] == '0') ? '1' : '0';
            inv[width] = '\0';            /* 字符串结束符 */
            printf("原码: ");
            printGrouped(orig);           /* 打印原码 */
            printf("反码: ");
            printGrouped(inv);            /* 打印反码 */
        } else {                          /* 绝对值 = 2^(n-1), 如 -128 */
            printf("说明: 该数绝对值为 2^%d, 超出原码/反码表示范围,\n"
                   "      只有补码形式(这是特殊值 %lld 的约定存储)。\n",
                   width - 1, value);     /* 解释 -2^(n-1) 没有原码/反码 */
        }
    } else {                              /* 正数 */
        printf("(正数的原码、反码、补码相同)\n"); /* 正数三种编码一致 */
    }
}

/*
 * intBinToDec: 功能 1 —— 二进制 -> 十进制 (整数)
 * 流程: 选位宽 -> 选解读方式(正数/负数绝对值/补码解读) -> 输入二进制 -> 输出结果
 */
static void intBinToDec(void)
{
    int width = chooseWidth();            /* 第一步: 选择位宽 */
    if (width < 0)                        /* EOF 则退出 */
        return;

    char line[128];                       /* 行缓冲区 */
    int mode = 0;                         /* 解读方式: 1=正数, 2=负数(绝对值), 3=补码解读 */
    while (mode < 1 || mode > 3) {        /* 循环直到输入 1/2/3 */
        printf("解读方式 (1=正数 / 2=负数(绝对值) / 3=补码解读): ");
        if (!readLine(line, sizeof line)) return;
        mode = atoi(line);                /* 将输入转为整数 */
        if (mode < 1 || mode > 3)
            printf("无效选择, 请输入 1、2 或 3。\n");
    }

    printf("请输入一个二进制数: ");
    if (!readLine(line, sizeof line))     /* 读取用户输入的二进制 */
        return;
    char bin[128];                        /* 去空格后的二进制字符串 */
    int n = stripSpaces(line, bin, sizeof bin); /* 去除空格, 返回有效位数 */
    uint64_t u;                           /* 解析后的无符号值 */
    if (n == 0 || n > 63 || !parseBinary(bin, &u)) { /* 校验: 非空、不超 63 位、仅含 0/1 */
        printf("输入错误: 请输入仅由 0/1 组成的数。\n");
        return;
    }

    if (mode == 1) {                      /* ---- 正数模式 ---- */
        uint64_t maxMag = (1ULL << (width - 1)) - 1; /* 有符号正数上限: 2^(n-1) - 1 */
        if (u > maxMag) {                 /* 超出正数范围 */
            printf("错误: 超出 %d 位正数范围 (最大 %llu)。\n",
                   width, (unsigned long long)maxMag);
            return;
        }
        printf("\n十进制值: +%llu\n\n", (unsigned long long)u); /* 输出正值 */
        printIntRep((long long)u, width); /* 打印原码/反码/补码 */
    } else if (mode == 2) {               /* ---- 负数(绝对值)模式 ---- */
        uint64_t maxMag = 1ULL << (width - 1); /* 最大绝对值: 2^(n-1), 即 |-2^(n-1)| */
        if (u > maxMag) {                 /* 绝对值超出范围 */
            printf("错误: 绝对值超出 %d 位负数范围 (最大绝对值 %llu)。\n",
                   width, (unsigned long long)maxMag);
            return;
        }
        long long value = -(long long)u;  /* 取负得到实际值 */
        printf("\n十进制值: %lld\n\n", value);
        if (u == 0)                       /* 特殊情况: -0 */
            printf("说明: 0 的补码为全0 (原码有 +0 和 -0 两种形式)。\n");
        printIntRep(value, width);        /* 打印原码/反码/补码 */
    } else {                              /* ---- 补码解读模式 ---- */
        if (n != width) {                 /* 补码解读必须恰好 width 位 */
            printf("错误: 补码解读模式需要恰好 %d 位 (你输入了 %d 位)。\n",
                   width, n);
            return;
        }
        /* 判断最高位: 为 1 则是负数, 实际值 = u - 2^width; 为 0 则是正数 */
        long long value = (u >> (width - 1)) ? (long long)(u - (1ULL << width)) : (long long)u;
        printf("\n十进制值: %lld\n\n", value);
        printIntRep(value, width);        /* 打印原码/反码/补码 */
    }
}

/*
 * intDecToBin: 功能 2 —— 十进制 -> 二进制 (整数)
 * 流程: 选位宽 -> 输入十进制数 -> 输出补码及原码/反码
 */
static void intDecToBin(void)
{
    int width = chooseWidth();            /* 选择位宽 */
    if (width < 0) return;

    long long minV = -(long long)(1ULL << (width - 1)); /* 有符号最小值: -2^(n-1) */
    long long maxV = (long long)((1ULL << (width - 1)) - 1); /* 有符号最大值: 2^(n-1) - 1 */
    long long v;
    while (1) {                           /* 循环直到输入合法 */
        printf("请输入一个十进制数 (%lld ~ %lld): ", minV, maxV);
        if (!readLongLong(&v)) {          /* 读取整数, 失败则提示 */
            printf("输入错误: 请输入一个合法整数。\n");
            continue;
        }
        if (v < minV || v > maxV) {       /* 超出位宽范围 */
            printf("错误: 超出 %d 位表示范围。\n", width);
            continue;
        }
        break;                            /* 输入合法, 跳出循环 */
    }

    /* 计算补码的无符号表示 */
    uint64_t u = (v < 0) ? ((1ULL << width) - (uint64_t)(-v)) : (uint64_t)v;
    char comp[64];
    toBinaryString(u, width, comp);       /* 转为二进制字符串 */
    printf("\n转换结果: %lld (十进制) = ", v);
    printGrouped(comp);                   /* 分组打印补码 */
    printf(" (二进制补码)\n\n");
    printIntRep(v, width);                /* 打印原码/反码/补码 */
}

/* ================= 浮点数转换功能 (IEEE 754) ================= */

/*
 * choosePrecision: 让用户选择浮点精度
 * 返回: 0=单精度(float, 32位), 1=双精度(double, 64位), -1=EOF
 * 说明: 用户输入 f 或 d (大小写均可), 前后允许空白
 */
static int choosePrecision(void)
{
    char line[64];                        /* 行缓冲区 */
    while (1) {
        printf("请选择精度 (f=单精度32位 / d=双精度64位): ");
        if (!readLine(line, sizeof line)) return -1;
        char *p = line;
        while (*p && isspace((unsigned char)*p)) /* 跳过前导空白 */
            p++;
        char c = (char)tolower((unsigned char)*p); /* 取第一个非空字符并转小写 */
        char *q = p + (*p ? 1 : 0);       /* q 指向 c 后面的字符 */
        while (*q && isspace((unsigned char)*q)) /* 跳过 c 后面的空白 */
            q++;
        if ((c == 'f' || c == 'd') && *q == '\0') { /* 合法: f 或 d, 且后面无其他字符 */
            printf("已选择: %s\n", c == 'f' ? "单精度 float (32位, IEEE 754)"
                                            : "双精度 double (64位, IEEE 754)");
            return (c == 'd') ? 1 : 0;    /* d->1(双精度), f->0(单精度) */
        }
        printf("输入无效, 请输入 f 或 d。\n");
    }
}

/*
 * printEncodingSteps: 展示十进制 -> IEEE 754 位模式的逐步编码推导过程
 * 参数: isDouble - 1=双精度(64位), 0=单精度(32位)
 *       bits - 编码完成后的位模式 (从中精确提取各字段, 避免浮点误差)
 *       val - 原始十进制值 (仅用于第1步展示)
 * 说明: 编码五步曲:
 *       第1步 展示十进制原值
 *       第2步 规格化: 化为 ±1.M × 2^k 形式, 确定真实指数 k
 *       第3步 指数域: E = k + bias, 转为二进制
 *       第4步 尾数域: 用"乘2取整法"逐位提取小数部分的二进制 (演示前8位)
 *       第5步 拼接 S | E | M 得到最终位模式
 * 注:   各字段从 bits 精确提取而非用浮点运算重新计算,
 *       因为 man/2^manBits 及其逐次×2 都是 2 的幂运算, 结果精确无误差
 */
static void printEncodingSteps(int isDouble, uint64_t bits, double val)
{
    int expBits = isDouble ? 11 : 8;      /* 指数位位数: double=11, float=8 */
    int manBits = isDouble ? 52 : 23;     /* 尾数位位数: double=52, float=23 */
    int bias = isDouble ? 1023 : 127;     /* 指数偏移量: double=1023, float=127 */
    uint64_t expMask = (1ULL << expBits) - 1; /* 指数位掩码 */

    /* 从位模式中分离三个字段 */
    uint64_t sign = bits >> (expBits + manBits);   /* 符号位 */
    uint64_t exp = (bits >> manBits) & expMask;    /* 指数域 */
    uint64_t man = bits & ((1ULL << manBits) - 1); /* 尾数域 */
    long long realExp = (long long)exp - bias;     /* 真实指数 = E - bias */

    char expB[12], manB[53];              /* 指数域/尾数域的二进制字符串缓冲区 */
    toBinaryString(exp, expBits, expB);   /* 指数域转二进制字符串 */
    toBinaryString(man, manBits, manB);   /* 尾数域转二进制字符串 */

    printf("\n----- 编码过程 (逐步推导) -----\n");

    /* 特殊值 (0 / 无穷大 / NaN / 非规格化数) 无法按常规五步推导, 单独说明 */
    if (exp == expMask || exp == 0) {
        printf("第1步 原值: %.17g\n", val);
        printf("说明: 该值为特殊编码(零/非规格化数/无穷大/NaN),\n"
               "      不适用常规规格化步骤, 直接按 IEEE 754 约定编码\n"
               "      (详见下方存储结构与数值验证)。\n");
        return;
    }

    /* ---- 第1步: 展示十进制原值 ---- */
    printf("第1步 原值: %.17g\n", val);

    /* ---- 第2步: 规格化, 确定真实指数 ---- */
    double fracVal = 1.0 + (double)man / (double)(1ULL << manBits); /* 1.M 的精确十进制值 */
    printf("第2步 规格化: = %s%.17g × 2^%lld\n",
           sign ? "-" : "", fracVal, realExp);
    printf("      (即二进制小数点从原位置移动 %lld 位, 隐去首位固定的 1)\n", realExp);

    /* ---- 第3步: 求指数域 ---- */
    printf("第3步 指数域: 真实指数 %lld + 偏移量 %d = %llu\n",
           realExp, bias, (unsigned long long)exp);
    printf("      E = %llu = %s (二进制, %d 位)\n",
           (unsigned long long)exp, expB, expBits);

    /* ---- 第4步: 求尾数域 (乘2取整法演示) ---- */
    double f = (double)man / (double)(1ULL << manBits); /* 小数部分 0.M, 精确值 */
    printf("第4步 尾数域: 对小数部分 %.10g 用\"乘2取整法\"逐位提取二进制:\n", f);
    int show = (manBits < 8) ? manBits : 8; /* 只演示前 8 位, 其余同理省略 */
    for (int i = 0; i < show; i++) {    /* 逐位: ×2 -> 取整数部分 -> 留小数部分 */
        double prev = f;                /* 记录乘 2 之前的小数, 用于展示 */
        f *= 2;                         /* 乘 2 (2 的幂运算, 精确无误差) */
        int bit = (f >= 1.0);           /* 整数部分即为该位二进制值 */
        if (bit) f -= 1.0;              /* 去掉整数部分, 留下小数部分继续 */
        printf("      %d) %.10g × 2 = %.10g → 取 %d, 余 %.10g\n",
               i + 1, prev, prev * 2, bit, f);
    }
    if (manBits > show)                 /* 位数多时提示后面省略 */
        printf("      ...(其余 %d 位同理, 此处省略)...\n", manBits - show);
    printf("      最终得尾数域 M = %s\n", manB);

    /* ---- 第5步: 拼接三个字段 ---- */
    printf("第5步 拼接: %llu | %s | %s\n",
           (unsigned long long)sign, expB, manB);
    if (isDouble)
        printf("      → 0x%016llX (见下方存储结构)\n", (unsigned long long)bits);
    else
        printf("      → 0x%08llX (见下方存储结构)\n", (unsigned long long)bits);
}

/*
 * printDecodingSteps: 展示 IEEE 754 位模式 -> 十进制数值的逐步解码推导过程
 * 参数: isDouble - 1=双精度(64位), 0=单精度(32位)
 *       bits - 输入的位模式
 * 说明: 解码六步曲:
 *       第1步 接收位模式并拆分 S/E/M 三个字段
 *       第2步 解码符号位: S=0 为正, S=1 为负
 *       第3步 解码指数域: E - bias = 真实指数, 并算出 2^真实指数 的十进制值
 *       第4步 解码尾数域: 按权展开, 列出每个为 1 的位及其十进制权值, 求和得 1.M
 *       第5步 组装: (-1)^S × 1.M × 2^真实指数, 全部用十进制逐步乘出最终结果
 *       第6步 与"数值验证"部分交叉核对 (闭环)
 */
static void printDecodingSteps(int isDouble, uint64_t bits)
{
    int expBits = isDouble ? 11 : 8;      /* 指数位位数: double=11, float=8 */
    int manBits = isDouble ? 52 : 23;     /* 尾数位位数: double=52, float=23 */
    int bias = isDouble ? 1023 : 127;     /* 指数偏移量: double=1023, float=127 */
    uint64_t expMask = (1ULL << expBits) - 1; /* 指数位掩码 */

    /* 从位模式中分离三个字段 */
    uint64_t sign = bits >> (expBits + manBits);   /* 符号位 */
    uint64_t exp = (bits >> manBits) & expMask;    /* 指数域 */
    uint64_t man = bits & ((1ULL << manBits) - 1); /* 尾数域 */

    char manB[53];                        /* 尾数域二进制字符串缓冲区 */
    toBinaryString(man, manBits, manB);   /* 尾数域转字符串, 便于逐位检查 */

    printf("\n----- 解码过程 (逐步推导) -----\n");

    /* ---- 第1步: 拆分字段 ---- */
    printf("第1步 拆分字段: S | E | M = %llu | ", (unsigned long long)sign);
    { /* 指数域二进制逐字打印 */
        char expB[12];
        toBinaryString(exp, expBits, expB);
        fputs(expB, stdout);
    }
    printf(" | %s\n", manB);

    /* 特殊值 (零/非规格化数/无穷大/NaN) 走专门规则, 不按常规六步推导 */
    if (exp == expMask || exp == 0) {
        printf("说明: 指数域为全0或全1, 属于特殊编码(零/非规格化数/无穷大/NaN),\n"
               "      按IEEE 754专门规则解读 (详见下方数值验证)。\n");
        return;
    }

    /* ---- 第2步: 解码符号位 ---- */
    printf("第2步 解码符号: S = %llu → %s\n", (unsigned long long)sign,
           sign ? "负数 (取 -1)" : "正数 (取 +1)");

    /* ---- 第3步: 解码指数域 ---- */
    long long realExp = (long long)exp - bias;      /* 真实指数 = E - bias */
    double p2 = pow(2.0, (double)realExp);          /* 2^真实指数 的十进制值 */
    printf("第3步 解码指数: E = %llu (十进制)\n", (unsigned long long)exp);
    printf("      真实指数 = E - 偏移量 = %llu - %d = %lld\n",
           (unsigned long long)exp, bias, realExp);
    printf("      即要乘以 2^%lld = %.10g (十进制)\n", realExp, p2);

    /* ---- 第4步: 解码尾数域 (按权展开, 十进制逐项累加) ---- */
    printf("第4步 解码尾数: 隐含位 1 + 各为1的位按权 2^-i 展开:\n");
    printf("      1 (隐含位) = 1\n");
    double frac = 1.0;                    /* 1.M 的十进制累加器, 从隐含位 1 开始 */
    int shown = 0;                        /* 已展示的项数 (最多展示 8 项) */
    for (int i = 0; i < manBits; i++) {   /* 遍历每个尾数位 */
        if (manB[i] == '1') {             /* 该位为 1 则其权值计入累加和 */
            double w = pow(2.0, (double)(-(i + 1))); /* 第 i 位权值 = 2^-(i+1) */
            frac += w;                    /* 累加进 1.M */
            if (shown < 8)                /* 项数太多时只演示前 8 项 */
                printf("      第%2d位 = 1 → 2^-%d = %.10g\n", i + 1, i + 1, w);
            shown++;
        }
    }
    if (shown > 8)                        /* 提示后面省略的项数 */
        printf("      ...(其余 %d 个为 1 的位同理累加)...\n", shown - 8);
    printf("      求和: 1.M = %.10g (十进制)\n", frac);

    /* ---- 第5步: 组装最终数值 (全部十进制逐步计算) ---- */
    printf("第5步 组装: (-1)^%llu × %.10g × 2^%lld\n",
           (unsigned long long)sign, frac, realExp);
    printf("      = %s%.10g × %.10g\n", sign ? "-" : "", frac, p2);
    printf("      = %s%.10g\n", sign ? "-" : "", frac * p2); /* 十进制直接乘出结果 */

    printf("第6步 核对: 与下方\"数值验证\"反向验算结果一致即解码无误。\n");
}

/*
 * printFloatFields: 拆解并打印 IEEE 754 浮点数的各字段
 * 参数: isDouble - 1=双精度(64位), 0=单精度(32位)
 *       bits - 浮点数的原始位模式 (作为无符号整数传入)
 *       userInputIsDec - 1=用户输入是十进制(选项3), 0=用户输入是二进制/十六进制(选项4)
 *       decVal - 该浮点数的十进制值 (反向验算时要比对回去的目标数值)
 * 输出:
 *   - 存储结构: 完整二进制(分组显示) / S-E-M 三段 / 十六进制 / 各字段十进制值
 *   - 数值验证: 从被转换成的进制出发, 反向验算回用户输入的数值:
 *       输入为十进制(选项3): 二进制结果 --按权展开--> 十进制, 与输入值比对
 *       输入为二进制(选项4): 十进制结果 --除2取余/乘2取整--> 二进制, 与输入位模式比对
 */
static void printFloatFields(int isDouble, uint64_t bits, int userInputIsDec, double decVal)
{
    /* 根据精度确定各字段的位宽和偏移量 */
    int expBits = isDouble ? 11 : 8;      /* 指数位位数: double=11, float=8 */
    int manBits = isDouble ? 52 : 23;     /* 尾数位位数: double=52, float=23 */
    int bias = isDouble ? 1023 : 127;     /* 指数偏移量: double=1023, float=127 */
    uint64_t expMask = (1ULL << expBits) - 1; /* 指数位掩码, 如 float: 0xFF */

    /* 从位模式中分离三个字段 */
    uint64_t sign = bits >> (expBits + manBits); /* 符号位: 最高 1 位 */
    uint64_t exp = (bits >> manBits) & expMask;  /* 指数位: 中间 expBits 位 */
    uint64_t man = bits & ((1ULL << manBits) - 1); /* 尾数位: 最低 manBits 位 */

    /* 构造完整的二进制字符串 */
    int total = expBits + manBits + 1;    /* 总位数: 32 或 64 */
    char full[65], expB[12], manB[53];    /* 缓冲区: full最大65, expB最大12, manB最大53 */
    toBinaryString(bits, total, full);    /* 全部位转为 "0101..." 字符串 */
    for (int i = 0; i < expBits; i++)     /* 截取指数位部分 */
        expB[i] = full[1 + i];            /* 从第 2 个字符开始 (跳过符号位) */
    expB[expBits] = '\0';
    for (int i = 0; i < manBits; i++)     /* 截取尾数位部分 */
        manB[i] = full[1 + expBits + i];  /* 从符号位+指数位之后开始 */
    manB[manBits] = '\0';

    /* 打印存储结构 */
    printf("\n----- IEEE 754 %s 存储结构 -----\n", isDouble ? "双精度(64位)" : "单精度(32位)");
    printf("完整二进制 : ");
    printGrouped(full);                   /* 每 4 位一组打印完整二进制 */
    printf("字段 S-E-M : %c | %s | %s\n", full[0], expB, manB); /* 三段分离显示 */
    if (isDouble)
        printf("十六进制   : 0x%016llX\n", (unsigned long long)bits); /* 64位: 16个十六进制数字 */
    else
        printf("十六进制   : 0x%08llX\n", (unsigned long long)bits); /* 32位: 8个十六进制数字 */
    printf("符号位 S = %llu (%s)\n", (unsigned long long)sign, sign ? "负" : "正"); /* 0=正, 1=负 */
    printf("指数位 E = %s (二进制) = %llu (十进制)\n", expB, (unsigned long long)exp); /* 指数的无偏移原始值 */
    printf("尾数位 M = %s\n", manB);

    /* 数值验证: 从位模式反推数值, 根据指数位判断是规格化/非规格化/特殊值 */
    printf("\n----- 数值验证 -----\n");
    if (exp == expMask) {                 /* 指数位全 1: 特殊值 */
        if (man == 0)                     /* 尾数全 0 -> 无穷大 */
            printf("特殊值: %s无穷大 (Infinity)\n", sign ? "负" : "正");
        else                              /* 尾数非 0 -> NaN */
            printf("特殊值: 非数 (NaN)\n");
    } else if (exp == 0) {                /* 指数位全 0 */
        if (man == 0)                     /* 尾数也全 0 -> 零 */
            printf("特殊值: %s零\n", sign ? "负" : "正");
        else                              /* 尾数非 0 -> 非规格化数 (次正规数) */
            printf("非规格化数: 值 = (-1)^%llu × 0.%s × 2^%d\n",
                   (unsigned long long)sign, manB, 1 - bias);
            /* 非规格化数: 隐含位为 0 (不是 1), 指数固定为 1-bias (不是 0-bias) */
    } else {                              /* 正常情况: 规格化数 */
        long long realExp = (long long)exp - bias; /* 真实指数 = E - bias */
        printf("规格化数: 值 = (-1)^%llu × 1.%s × 2^(%llu - %d)\n",
               (unsigned long long)sign, manB, (unsigned long long)exp, bias);
        printf("        = (-1)^%llu × 1.%s × 2^%lld\n",
               (unsigned long long)sign, manB, realExp);
        /* 规格化数: 隐含前导 1, 真实指数 = E - bias */

        /* ---- 小数点移位: 把 1.M 乘以 2^realExp 还原成真正的二进制数 ---- */
        char digits[80];                  /* 缓冲区: 隐含位 '1' + 尾数位, double 最多 53 位 */
        digits[0] = '1';                  /* 隐含的前导 1 */
        memcpy(digits + 1, manB, (size_t)manBits); /* digits = "1" + M, 共 manBits+1 位 */
        int digitsLen = manBits + 1;      /* 有效数字总长度 */
        printf("        = (");
        if (realExp >= 0) {               /* 指数 >= 0: 小数点向右移, 可能在末尾补 0 */
            int pt = 1 + (int)realExp;    /* 小数点应在 digits 中第 pt 个字符之后 */
            for (int i = 0; i < digitsLen; i++) { /* 逐位输出整数和小数部分 */
                if (i == pt) putchar('.');/* 到达小数点位置时插入 '.' */
                putchar(digits[i]);
            }
            for (int i = digitsLen; i < pt; i++) /* 位不够时整数部分低位补 0 */
                putchar('0');
            if (digitsLen <= pt)          /* 没有任何小数位时补 ".0" 保持形态清晰 */
                fputs(".0", stdout);
        } else {                          /* 指数 < 0: 小数点向左移, 整数部分变为 0 */
            fputs("0.", stdout);          /* 形如 1.101×2^-2 -> 0.01101 */
            for (long long i = 0; i < -realExp - 1; i++) /* 先补 -realExp-1 个前导 0 */
                putchar('0');
            fputs(digits, stdout);        /* 再输出全部有效数字 */
        }
        printf(")(二进制)\n");

        /* ---- 裁剪版: 去掉末尾无效的 0, 展示数学上最简的二进制原码 ---- */
        /* 注意: 指数极大/极小时二进制串可达上千字符, 缓冲区需足够大 (1200) */
        char trimmed[1200];               /* 复制移位结果用于裁剪 */
        int t = 0;
        /* 重新生成移位后的字符串(与上面逻辑相同, 只是不直接打印) */
        if (realExp >= 0) {
            int pt = 1 + (int)realExp;
            for (int i = 0; i < digitsLen; i++) {
                if (i == pt) trimmed[t++] = '.';
                trimmed[t++] = digits[i];
            }
            for (int i = digitsLen; i < pt; i++)
                trimmed[t++] = '0';
            if (digitsLen <= pt) { trimmed[t++] = '.'; trimmed[t++] = '0'; }
        } else {
            trimmed[t++] = '0'; trimmed[t++] = '.';
            for (long long i = 0; i < -realExp - 1; i++) trimmed[t++] = '0';
            for (int i = 0; i < digitsLen; i++) trimmed[t++] = digits[i];
        }
        trimmed[t] = '\0';
        /* 从末尾删 '0'; 若删到小数点则连小数点一起删 (如 1001.000 -> 1001) */
        while (t > 0 && trimmed[t - 1] == '0')
            trimmed[--t] = '\0';
        if (t > 0 && trimmed[t - 1] == '.')
            trimmed[--t] = '\0';
        printf("        最简形式: (%s)(二进制)\n", trimmed);

        /* ---- 计算 1.M 的十进制值与最终数值 (2 的幂运算, 全程精确) ---- */
        double frac = 1.0, w = 0.5;       /* frac 初始为隐含位 1; w 为当前位的权值 2^-1 */
        for (int i = 0; i < manBits; i++, w /= 2) /* 遍历每个尾数位 */
            if (manB[i] == '1')           /* 该位为 1 则累加其权值 */
                frac += w;
        double v = (sign ? -1.0 : 1.0) * frac * pow(2.0, (double)realExp); /* 最终数值 */

        if (userInputIsDec) {
            /* ==== 反向验算 (输入为十进制): 从二进制结果按权展开, 验算回十进制 ==== */
            printf("        验证(二进制→十进制, 按权展开):\n");
            printf("              %s\n", trimmed);
            double sum = 0.0;             /* 各位权值累加和 */
            int terms = 0;                /* 为 1 的位数计数 (限制展示数量) */
            char *dot = strchr(trimmed, '.');      /* 定位小数点 */
            int intLen = dot ? (int)(dot - trimmed) : (int)strlen(trimmed);
            for (int i = 0; i < intLen; i++) {     /* 整数部分: 第 i 位权值 = 2^(intLen-1-i) */
                if (trimmed[i] != '1') continue;   /* 为 0 的位不计数 */
                double wgt = pow(2.0, (double)(intLen - 1 - i)); /* 该位的权值 */
                sum += wgt;                        /* 累加 */
                if (terms < 8)                     /* 项数太多时只展示前 8 项 */
                    printf("            + 1×2^%d = %.10g\n", intLen - 1 - i, wgt);
                terms++;
            }
            if (dot) {                             /* 小数部分: 点后第 j 位权值 = 2^-(j+1) */
                int flen = (int)strlen(dot + 1);
                for (int j = 0; j < flen; j++) {
                    if (dot[1 + j] != '1') continue;
                    double wgt = pow(2.0, -(double)(j + 1));
                    sum += wgt;
                    if (terms < 8)
                        printf("            + 1×2^-%d = %.10g\n", j + 1, wgt);
                    terms++;
                }
            }
            if (terms > 8)                         /* 提示省略的项数 */
                printf("            ...(其余 %d 个为 1 的位同理累加)...\n", terms - 8);
            double total = sign ? -sum : sum;      /* 套上符号位得到最终值 */
            if (sign)                              /* 负数: 说明符号位的处理 */
                printf("            符号位 S=1 → 取负\n");
            printf("            求和 = %.10g", total);
            if (total == decVal)                   /* 精确相等: 全程 2 的幂运算无误差 */
                printf("  → 与输入的十进制值 %.10g 一致 ✓\n", decVal);
            else                                   /* 理论上不会发生, 保险提示 */
                printf("  → 与输入值 %.10g 存在差异!\n", decVal);
        } else {
            /* ==== 反向验算 (输入为二进制/十六进制): 从十进制结果反推回二进制 ==== */
            printf("        验证(十进制→二进制, 反向验算):\n");
            double av = fabs(decVal);              /* 绝对值 (符号位单独处理) */
            double ip = floor(av);                 /* 整数部分 */
            double fp = av - ip;                   /* 小数部分 */
            char rec[1200];                        /* 反推出的二进制串 */
            int t2 = 0;
            if (ip == 0.0) {                       /* 整数部分为 0 */
                printf("              整数部分 0 → 0\n");
                rec[t2++] = '0';
            } else {                               /* 整数部分: 除2取余, 余数倒序排列 */
                char rev[1200];                    /* 存放余数序列 (低位在前) */
                int rn = 0, shown = 0;
                double n = ip;
                printf("              整数部分 %.0f: 除2取余\n", ip);
                while (n >= 1.0 && rn < 1100) {    /* 不断除 2 直到商为 0 */
                    double half = n / 2.0;         /* 除以 2 (2 的幂运算, 精确) */
                    double fl = floor(half);       /* 商 (取整) */
                    int r = (int)(n - 2.0 * fl);   /* 余数 = 被除数 - 2×商, 必为 0 或 1 */
                    rev[rn++] = (char)('0' + r);   /* 记录余数 */
                    if (shown < 8)                 /* 步数太多时只展示前 8 步 */
                        printf("              %.0f ÷ 2 = %.0f 余 %d\n", n, fl, r);
                    shown++;
                    n = fl;                        /* 以商继续下一轮 */
                }
                if (shown > 8)
                    printf("              ...(其余 %d 步同理)...\n", shown - 8);
                while (rn > 0)                     /* 余数倒序写出 -> 高位在前 */
                    rec[t2++] = rev[--rn];
            }
            rec[t2++] = '.';                       /* 写入小数点 */
            if (fp == 0.0) {                       /* 无小数部分 */
                printf("              小数部分 0 → 无小数位\n");
                rec[t2++] = '0';                   /* 暂以 0 占位, 后续裁剪会去掉 */
            } else {                               /* 小数部分: 乘2取整, 顺序排列 */
                printf("              小数部分 %.10g: 乘2取整\n", fp);
                int shown = 0;
                while (fp > 0.0 && t2 < 1190 && shown < 1100) { /* 直到小数部分为 0 */
                    double prev = fp;              /* 记录乘 2 之前的小数, 用于展示 */
                    double d2 = fp * 2.0;          /* 乘 2 (2 的幂运算, 精确) */
                    int bit = (d2 >= 1.0);         /* 整数部分即为该位二进制值 */
                    fp = d2 - (double)bit;         /* 去掉整数部分, 留小数部分继续 */
                    rec[t2++] = (char)('0' + bit); /* 记录该位 */
                    if (shown < 8)                 /* 步数太多时只展示前 8 步 */
                        printf("              %.10g × 2 = %.10g → 取 %d, 余 %.10g\n",
                               prev, prev * 2.0, bit, fp);
                    shown++;
                }
                if (fp > 0.0)                      /* 保险分支: 二进制展开必终止, 不会触发 */
                    printf("              ...(已达精度上限, 后续省略)...\n");
                else if (shown > 8)
                    printf("              ...(其余 %d 步同理)...\n", shown - 8);
            }
            rec[t2] = '\0';
            /* 裁剪末尾的 0 和小数点 (与最简形式同规则, 便于比对) */
            while (t2 > 0 && rec[t2 - 1] == '0') rec[--t2] = '\0';
            if (t2 > 0 && rec[t2 - 1] == '.') rec[--t2] = '\0';
            printf("              组合: %s", rec);
            if (sign) printf(" (符号位 S=1 → 前置负号)");
            printf("\n");
            if (strcmp(rec, trimmed) == 0) {       /* 反推结果与解码结果比对 */
                printf("              与解码得到的最简形式 (%s) 一致,\n", trimmed);
                if (isDouble)
                    printf("              将其按 IEEE 754 编码即还原出输入位模式 0x%016llX ✓\n",
                           (unsigned long long)bits);
                else
                    printf("              将其按 IEEE 754 编码即还原出输入位模式 0x%08llX ✓\n",
                           (unsigned long long)bits);
            } else {                               /* 理论上不会发生, 保险提示 */
                printf("              与最简形式 (%s) 不一致, 请检查!\n", trimmed);
            }
        }
    }
}

/*
 * floatDecToBin: 功能 3 —— 十进制浮点数 -> 二进制 (IEEE 754)
 * 流程: 选精度(f/d) -> 输入十进制浮点数 -> 逐步推导编码过程 -> 输出 IEEE 754 位模式
 * 核心技巧: 用 memcpy 将 float/double 的内存位模式复制为 uint32_t/uint64_t
 *           这是 C99/C11 标准保证合法的"类型双关"(type punning)方式
 */
static void floatDecToBin(void)
{
    int isDouble = choosePrecision();     /* 选择单精度或双精度 */
    if (isDouble < 0) return;

    double val;
    while (1) {                           /* 循环直到输入合法 */
        printf("请输入一个十进制浮点数 (支持科学计数法: 12.5, -0.1, 1e-3, 2.5E8, 1.5×10^3): ");
        if (readDouble(&val))             /* 读取 double */
            break;
        printf("输入错误: 请输入一个合法的浮点数。\n");
    }

    if (isDouble) {                       /* ---- 双精度 (64位) ---- */
        uint64_t bits;
        memcpy(&bits, &val, sizeof bits); /* 将 double 的 8 字节内存原样复制为 uint64_t */
        printEncodingSteps(1, bits, val); /* 先展示逐步编码推导过程 */
        printFloatFields(1, bits, 1, val); /* 再展示存储结构与反向验算 (输入是十进制) */
        printf("\n转换结果: 0x%016llX (十六进制) = %.17g (十进制)\n",
               (unsigned long long)bits, val); /* 同时给出 16 进制位模式与十进制回读值 */
        /* double 约 15~17 位有效数字 */
    } else {                              /* ---- 单精度 (32位) ---- */
        float f = (float)val;             /* 将 double 截断为 float */
        /* 精度损失/溢出检测 */
        if (isfinite(val) && isinf((double)f)) /* 有限值转 float 后变无穷 -> 溢出 */
            printf("\n警告: 该值超出单精度范围(约±3.4e38), 将存储为无穷大!\n");
        else if (isfinite(val) && val != 0.0 && f == 0.0f) /* 非零值变 0 -> 下溢 */
            printf("\n警告: 该值绝对值太小, 单精度下下溢为 0!\n");
        else if (isfinite(val) && (double)f != val) /* 精度截断 */
            printf("\n提示: 单精度仅约7位有效数字, 实际存储 %.9g (存在精度损失)。\n",
                   (double)f);

        uint32_t b32;
        memcpy(&b32, &f, sizeof b32);     /* 将 float 的 4 字节内存原样复制为 uint32_t */
        printEncodingSteps(0, b32, (double)f); /* 先展示逐步编码推导过程 (基于实际存储的 f) */
        printFloatFields(0, b32, 1, (double)f); /* 再展示存储结构与反向验算 (输入是十进制) */
        printf("\n转换结果: 0x%08llX (十六进制) = %.9g (十进制)\n",
               (unsigned long long)b32, (double)f); /* 同时给出 16 进制位模式与十进制回读值 */
        /* float 约 7~9 位有效数字 */
    }
}

/*
 * floatBinToDec: 功能 4 —— 二进制 -> 十进制浮点数 (IEEE 754)
 * 流程: 选精度(f/d) -> 输入 32/64 位二进制串或 8/16 位十六进制串 -> 输出对应的浮点数值
 * 核心技巧: 将二进制/十六进制串解析为 uint32_t/uint64_t, 再用 memcpy 还原为 float/double
 * 输入格式自动识别:
 *   - 0x/0X 前缀          -> 按十六进制解析
 *   - 仅含 0/1 且长度匹配 -> 按二进制解析
 *   - 仅含 0-9/A-F/a-f 且长度为位数/4 -> 按十六进制解析
 */
static void floatBinToDec(void)
{
    int isDouble = choosePrecision();     /* 选择精度 */
    if (isDouble < 0) return;

    int need = isDouble ? 64 : 32;        /* 需要的二进制位数: 64 或 32 */
    int needHex = need / 4;               /* 对应的十六进制位数: 16 或 8 */
    char line[256], clean[80];            /* 行缓冲区和去空格后的输入串 */
    uint64_t bits = 0;                    /* 解析出的位模式 */
    while (1) {                           /* 循环直到输入合法 */
        printf("请输入 %d 位二进制数 或 %d 位十六进制数 (均可用空格分隔,\n"
               "十六进制可带 0x 前缀, 如 411A0000 或 0x411A0000): ", need, needHex);
        if (!readLine(line, sizeof line)) return;
        int n = stripSpaces(line, clean, sizeof clean); /* 去除所有空白 */
        if (n == 0) {
            printf("输入为空, 请重新输入。\n");
            continue;
        }

        /* ---- 情形 1: 0x/0X 前缀 -> 明确是十六进制 ---- */
        int isHex = (clean[0] == '0' && (clean[1] == 'x' || clean[1] == 'X'));

        /* ---- 情形 2: 无前缀时按内容自动判定 ---- */
        int only01 = 1, onlyHex = 1;      /* 两个探测标志 */
        for (int i = 0; i < n; i++) {     /* 逐字符检查 */
            char c = (char)tolower((unsigned char)clean[i]);
            if (clean[i] != '0' && clean[i] != '1') only01 = 0;   /* 不纯是 0/1 */
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) /* 不是hex字符 */
                onlyHex = 0;
        }

        if (isHex || (!only01 && onlyHex)) { /* ---- 按十六进制解析 ---- */
            if (isHex)
                n -= 2;                   /* 前缀不占长度 */
            if (n != needHex) {           /* hex 长度校验: 8 或 16 个字符 */
                printf("长度错误: 十六进制需要 %d 位, 你输入了 %d 位。\n",
                       needHex, n);
                continue;
            }
            if (!parseHex(clean, &bits)) { /* 解析(内部会再校验一次字符) */
                printf("字符错误: 十六进制只能包含 0-9 / A-F / a-f。\n");
                continue;
            }
            printf("已识别为十六进制输入。\n");
        } else if (only01) {                  /* ---- 按二进制解析 ---- */
            if (n != need) {              /* 二进制长度校验: 32 或 64 位 */
                printf("长度错误: 二进制需要 %d 位, 你输入了 %d 位。\n", need, n);
                continue;
            }
            bits = 0;
            for (int i = 0; i < n; i++)   /* 逐位左移累加 */
                bits = (bits << 1) | (uint64_t)(clean[i] - '0');
            printf("已识别为二进制输入。\n");
        } else {                          /* ---- 两种格式都不匹配 ---- */
            printf("输入错误: 请输入 %d 位二进制数 或 %d 位十六进制数。\n",
                   need, needHex);
            continue;
        }
        break;                            /* 校验通过, 跳出循环 */
    }

    /* 先将位模式还原为浮点数 (供反向验算与最终结果共同使用) */
    double decVal = 0.0;                  /* 该位模式对应的十进制值 */
    if (isDouble) {
        memcpy(&decVal, &bits, sizeof decVal); /* uint64_t 的 8 字节原样复制为 double */
    } else {
        uint32_t b32 = (uint32_t)bits;    /* 截取低 32 位 */
        float f;
        memcpy(&f, &b32, sizeof f);       /* uint32_t 的 4 字节原样复制为 float */
        decVal = (double)f;               /* 提升为 double 参与后续展示与验算 */
    }

    printDecodingSteps(isDouble, bits);   /* 先展示逐步解码推导过程 */
    printFloatFields(isDouble, bits, 0, decVal); /* 再展示存储结构与反向验算 (输入是二进制) */

    /* 输出最终转换结果 */
    if (isDouble)
        printf("\n转换结果: 0x%016llX (十六进制) = %.17g (十进制 double)\n",
               (unsigned long long)bits, decVal); /* 同步给出 16 进制与十进制结果 */
    else
        printf("\n转换结果: 0x%08llX (十六进制) = %.9g (十进制 float)\n",
               (unsigned long long)bits, decVal); /* 同步给出 16 进制与十进制结果 */
}

/* ================= 主菜单 ================= */

/*
 * main: 程序入口, 提供循环菜单供用户选择功能
 * 功能列表:
 *   1 - 整数: 二进制 -> 十进制
 *   2 - 整数: 十进制 -> 二进制
 *   3 - 浮点: 十进制 -> 二进制 (IEEE 754)
 *   4 - 浮点: 二进制 -> 十进制 (IEEE 754)
 *   0 - 退出程序
 */
int main(void)
{
    int choice = -1;                      /* 用户选择, 初始化为 -1 确保进入循环 */
    while (choice != 0) {                 /* 主循环, 选 0 退出 */
        printf("\n===== 进制转换工具 =====\n");
        printf(" 1. 整数: 二进制 -> 十进制\n");
        printf(" 2. 整数: 十进制 -> 二进制\n");
        printf(" 3. 浮点: 十进制 -> 二进制 (IEEE 754)\n");
        printf(" 4. 浮点: 二进制 -> 十进制 (IEEE 754)\n");
        printf(" 0. 退出\n");
        printf("========================\n");
        printf("请选择功能 (0-4): ");
        char line[64];
        if (!readLine(line, sizeof line)) /* 读取选择, EOF 则退出 */
            break;
        choice = atoi(line);              /* 将输入转为整数 */
        switch (choice) {                 /* 根据选择调用对应功能 */
            case 1: intBinToDec();  break; /* 整数: 二进制 -> 十进制 */
            case 2: intDecToBin();  break; /* 整数: 十进制 -> 二进制 */
            case 3: floatDecToBin(); break; /* 浮点: 十进制 -> 二进制 */
            case 4: floatBinToDec(); break; /* 浮点: 二进制 -> 十进制 */
            case 0: break;                /* 退出 */
            default:                      /* 无效选择 */
                printf("无效选择, 请重新输入。\n");
                choice = -1;              /* 重置为 -1, 继续循环 */
        }
    }
    printf("已退出。再见!\n");
    return 0;                             /* 正常退出, 返回 0 */
}
