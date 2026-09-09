/*
 * converter.c —— 进制转换与编码演示工具 (含 IEEE 754 浮点)
 *
 * 功能:
 *   1. 整数: 二进制 -> 十进制 (正数 / 负数绝对值 / 补码解读, 8/16/32位)
 *   2. 整数: 十进制 -> 二进制 (原码/反码/补码)
 *   3. 浮点: 十进制 -> 二进制 (IEEE 754, f=单精度32位 / d=双精度64位)
 *   4. 浮点: 二进制 -> 十进制 (IEEE 754, f=单精度32位 / d=双精度64位)
 *
 * 编译: gcc converter.c -o converter -lm
 */

/* ================= 头文件 ================= */

#include <stdio.h>      /* 标准输入输出: printf, scanf, fgets, putchar 等 */
#include <stdlib.h>     /* 通用工具: atoi, strtol, strtod 等字符串转数值函数 */
#include <string.h>     /* 字符串操作: strlen, memcpy 等 */
#include <stdint.h>     /* 固定宽度整数类型: uint32_t, uint64_t, int64_t 等 */
#include <ctype.h>      /* 字符分类: isspace, tolower 等 */
#include <errno.h>      /* 错误码: ERANGE (数值溢出时 strtoll 设置此值) */
#include <math.h>       /* 数学函数: isfinite, isinf (用于浮点溢出检测) */

/* ================= 通用工具函数 ================= */

/*
 * readLine: 从标准输入读取一整行文本
 * 参数: buf - 存储读取内容的字符数组, size - 数组最大容量
 * 返回: 成功读取返回 1, 遇到 EOF 返回 0
 * 说明: 自动去除行尾的换行符 \n 和回车符 \r (兼容 Windows/Linux)
 */
static int readLine(char *buf, size_t size)
{
    if (!fgets(buf, (int)size, stdin))  /* fgets 读取一行; 失败(EOF)则返回 0 */
        return 0;
    size_t len = strlen(buf);           /* 获取读入字符串的实际长度 */
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';              /* 从末尾逐个去除 \n 和 \r */
    return 1;                           /* 成功 */
}

/*
 * stripSpaces: 去除字符串中的所有空白字符(空格/制表符等)
 * 参数: src - 原始字符串, dst - 输出缓冲区, dstSize - 缓冲区大小
 * 返回: 去除空白后的有效字符数
 * 用途: 让用户输入二进制时可以加空格分隔, 如 "1010 1100" -> "10101100"
 */
static int stripSpaces(const char *src, char *dst, size_t dstSize)
{
    size_t n = 0;                       /* 记录写入 dst 的字符数 */
    for (const char *p = src; *p && n + 1 < dstSize; p++) {  /* 遍历源字符串每个字符 */
        if (!isspace((unsigned char)*p)) /* 如果不是空白字符 */
            dst[n++] = *p;              /* 复制到目标缓冲区 */
    }
    dst[n] = '\0';                      /* 添加字符串结束符 */
    return (int)n;                      /* 返回有效长度 */
}

/*
 * parseBinary: 将纯二进制字符串解析为 uint64 无符号整数
 * 参数: s - 仅含 '0'/'1' 的字符串, out - 输出解析结果
 * 返回: 成功返回 1, 字符串为空或含非法字符返回 0
 * 说明: 使用左移+或运算逐位累加, 如 "101" -> (1<<2)|(0<<1)|(1<<0) = 5
 */
static int parseBinary(const char *s, uint64_t *out)
{
    if (!*s)                            /* 空字符串, 解析失败 */
        return 0;
    uint64_t v = 0;                     /* 累加器, 初始为 0 */
    for (const char *p = s; *p; p++) {  /* 从左到右遍历每个字符 */
        if (*p != '0' && *p != '1')     /* 遇到非 0/1 字符 */
            return 0;                   /* 解析失败 */
        v = (v << 1) | (uint64_t)(*p - '0');  /* 左移一位腾出最低位, 或上当前位的值 */
    }
    *out = v;                           /* 将结果写入输出参数 */
    return 1;                           /* 成功 */
}

/*
 * toBinaryString: 将无符号整数转换为指定位宽的二进制字符串
 * 参数: val - 要转换的数值, bits - 目标位宽, out - 输出字符数组(需至少 bits+1 字节)
 * 说明: 从最高位开始逐位提取, 不足位宽时高位自动补 0
 *       例如 toBinaryString(5, 8, out) -> out = "00000101"
 */
static void toBinaryString(uint64_t val, int bits, char *out)
{
    for (int i = bits - 1; i >= 0; i--)         /* 从最高位(bits-1)遍历到最低位(0) */
        out[bits - 1 - i] = ((val >> i) & 1) ? '1' : '0';  /* 右移 i 位后取最低位 */
    out[bits] = '\0';                           /* 字符串结束符 */
}

/*
 * printGrouped: 按每 4 位一组打印二进制字符串, 组间加空格
 * 参数: s - 二进制字符串 (如 "00000101")
 * 输出: 格式化打印 (如 "0000 0101")
 * 说明: 从右往左数, 每 4 位插入一个空格, 便于阅读
 */
static void printGrouped(const char *s)
{
    int len = (int)strlen(s);           /* 获取字符串总长度 */
    for (int i = 0; i < len; i++) {     /* 逐字符遍历 */
        if (i > 0 && (len - i) % 4 == 0)/* 如果当前位置右侧剩余字符数是 4 的倍数 */
            putchar(' ');               /* 在当前字符前插入一个空格 */
        putchar(s[i]);                  /* 打印当前字符 */
    }
    putchar('\n');                      /* 末尾换行 */
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
    char line[256];                     /* 行缓冲区 */
    if (!readLine(line, sizeof line))   /* 读取一行 */
        return 0;
    errno = 0;                          /* 清除之前的错误码 */
    char *end = NULL;                   /* end 将指向第一个无法解析的字符 */
    long long v = strtoll(line, &end, 10);  /* 以 10 进制解析字符串为 long long */
    if (end == line || errno == ERANGE) /* end==line 表示没解析到任何数字; ERANGE 表示溢出 */
        return 0;
    while (*end && isspace((unsigned char)*end))  /* 跳过尾随空白 */
        end++;
    if (*end)                           /* 如果还有非空白字符残留 (如 "123abc") */
        return 0;                       /* 视为非法输入 */
    *out = v;                           /* 写入结果 */
    return 1;
}

/*
 * readDouble: 从标准输入读取一个 double 浮点数 (带严格校验)
 * 参数: out - 输出读取到的浮点值
 * 返回: 成功返回 1, 输入非法返回 0
 * 说明: 使用 strtod 解析, 支持小数点、科学计数法 (如 1.5e-3)、inf、nan
 *       校验逻辑与 readLongLong 相同: 空输入/无有效字符/尾随垃圾字符均拒绝
 */
static int readDouble(double *out)
{
    char line[256];                     /* 行缓冲区 */
    if (!readLine(line, sizeof line))   /* 读取一行 */
        return 0;
    char *end = NULL;                   /* 指向第一个无法解析的字符 */
    double v = strtod(line, &end);      /* 解析字符串为 double */
    if (end == line)                    /* 没解析到任何有效数字 */
        return 0;
    while (*end && isspace((unsigned char)*end))  /* 跳过尾随空白 */
        end++;
    if (*end)                           /* 有非空白垃圾字符残留 */
        return 0;
    *out = v;                           /* 写入结果 */
    return 1;
}

/* ================= 整数转换功能 ================= */

/*
 * chooseWidth: 让用户选择位宽 (8/16/32)
 * 返回: 8、16 或 32; 输入无效时循环提示; 遇到 EOF 返回 -1
 */
static int chooseWidth(void)
{
    char line[64];                      /* 行缓冲区 */
    while (1) {                         /* 循环直到输入合法 */
        printf("请选择位宽 (8/16/32): ");
        if (!readLine(line, sizeof line))  /* 读取一行, EOF 则退出 */
            return -1;
        char *end = NULL;
        long w = strtol(line, &end, 10);  /* 解析为 long */
        while (*end && isspace((unsigned char)*end))  /* 跳过尾随空白 */
            end++;
        if (*end == '\0' && (w == 8 || w == 16 || w == 32)) {  /* 合法: 无残留且值为 8/16/32 */
            printf("已选择: %ld 位位宽\n", w);
            return (int)w;              /* 返回位宽 */
        }
        printf("输入无效, 请输入 8、16 或 32。\n");  /* 不合法则提示重新输入 */
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
    uint64_t range = 1ULL << width;     /* 2^width, 即该位宽的总表示范围 */
    /* 计算补码的无符号表示: 负数用 2^n - |value|, 正数直接用原值 */
    uint64_t u = (value < 0) ? (range - (uint64_t)(-value)) : (uint64_t)value;
    char comp[64];                      /* 存放补码二进制字符串 */
    toBinaryString(u, width, comp);     /* 转换为二进制字符串 */
    printf("补码: ");
    printGrouped(comp);                 /* 分组打印补码 */

    if (value < 0) {                    /* 负数: 额外展示原码和反码 */
        uint64_t m = (uint64_t)(-value);          /* 绝对值 */
        uint64_t maxMag = (range >> 1) - 1;       /* 原码/反码能表示的最大绝对值 = 2^(n-1) - 1 */
        if (m <= maxMag) {              /* 绝对值在原码/反码范围内 */
            char orig[64], inv[64];     /* 原码和反码的字符串缓冲区 */
            orig[0] = '1';             /* 原码符号位 = 1 (负数) */
            toBinaryString(m, width - 1, orig + 1);  /* 原码数值位 = 绝对值的二进制 */
            inv[0] = '1';              /* 反码符号位 = 1 (负数) */
            for (int i = 1; i < width; i++)       /* 反码数值位 = 原码数值位逐位取反 */
                inv[i] = (orig[i] == '0') ? '1' : '0';
            inv[width] = '\0';         /* 字符串结束符 */
            printf("原码: ");
            printGrouped(orig);         /* 打印原码 */
            printf("反码: ");
            printGrouped(inv);          /* 打印反码 */
        } else {                        /* 绝对值 = 2^(n-1), 如 -128 */
            printf("说明: 该数绝对值为 2^%d, 超出原码/反码表示范围,\n"
                   "      只有补码形式(这是特殊值 %lld 的约定存储)。\n",
                   width - 1, value);  /* 解释 -2^(n-1) 没有原码/反码 */
        }
    } else {                            /* 正数 */
        printf("(正数的原码、反码、补码相同)\n");  /* 正数三种编码一致 */
    }
}

/*
 * intBinToDec: 功能 1 —— 二进制 -> 十进制 (整数)
 * 流程: 选位宽 -> 选解读方式(正数/负数绝对值/补码解读) -> 输入二进制 -> 输出结果
 */
static void intBinToDec(void)
{
    int width = chooseWidth();          /* 第一步: 选择位宽 */
    if (width < 0)                      /* EOF 则退出 */
        return;

    char line[128];                     /* 行缓冲区 */
    int mode = 0;                       /* 解读方式: 1=正数, 2=负数(绝对值), 3=补码解读 */
    while (mode < 1 || mode > 3) {      /* 循环直到输入 1/2/3 */
        printf("解读方式 (1=正数 / 2=负数(绝对值) / 3=补码解读): ");
        if (!readLine(line, sizeof line))
            return;
        mode = atoi(line);              /* 将输入转为整数 */
        if (mode < 1 || mode > 3)
            printf("无效选择, 请输入 1、2 或 3。\n");
    }

    printf("请输入一个二进制数: ");
    if (!readLine(line, sizeof line))   /* 读取用户输入的二进制 */
        return;
    char bin[128];                      /* 去空格后的二进制字符串 */
    int n = stripSpaces(line, bin, sizeof bin);  /* 去除空格, 返回有效位数 */

    uint64_t u;                         /* 解析后的无符号值 */
    if (n == 0 || n > 63 || !parseBinary(bin, &u)) {  /* 校验: 非空、不超 63 位、仅含 0/1 */
        printf("输入错误: 请输入仅由 0/1 组成的数。\n");
        return;
    }

    if (mode == 1) {                    /* ---- 正数模式 ---- */
        uint64_t maxMag = (1ULL << (width - 1)) - 1;  /* 有符号正数上限: 2^(n-1) - 1 */
        if (u > maxMag) {               /* 超出正数范围 */
            printf("错误: 超出 %d 位正数范围 (最大 %llu)。\n",
                   width, (unsigned long long)maxMag);
            return;
        }
        printf("\n十进制值: +%llu\n\n", (unsigned long long)u);  /* 输出正值 */
        printIntRep((long long)u, width);  /* 打印原码/反码/补码 */

    } else if (mode == 2) {             /* ---- 负数(绝对值)模式 ---- */
        uint64_t maxMag = 1ULL << (width - 1);  /* 最大绝对值: 2^(n-1), 即 |-2^(n-1)| */
        if (u > maxMag) {               /* 绝对值超出范围 */
            printf("错误: 绝对值超出 %d 位负数范围 (最大绝对值 %llu)。\n",
                   width, (unsigned long long)maxMag);
            return;
        }
        long long value = -(long long)u;  /* 取负得到实际值 */
        printf("\n十进制值: %lld\n\n", value);
        if (u == 0)                     /* 特殊情况: -0 */
            printf("说明: 0 的补码为全0 (原码有 +0 和 -0 两种形式)。\n");
        printIntRep(value, width);      /* 打印原码/反码/补码 */

    } else {                            /* ---- 补码解读模式 ---- */
        if (n != width) {               /* 补码解读必须恰好 width 位 */
            printf("错误: 补码解读模式需要恰好 %d 位 (你输入了 %d 位)。\n", width, n);
            return;
        }
        /* 判断最高位: 为 1 则是负数, 实际值 = u - 2^width; 为 0 则是正数 */
        long long value = (u >> (width - 1))
                              ? (long long)(u - (1ULL << width))
                              : (long long)u;
        printf("\n十进制值: %lld\n\n", value);
        printIntRep(value, width);      /* 打印原码/反码/补码 */
    }
}

/*
 * intDecToBin: 功能 2 —— 十进制 -> 二进制 (整数)
 * 流程: 选位宽 -> 输入十进制数 -> 输出补码及原码/反码
 */
static void intDecToBin(void)
{
    int width = chooseWidth();          /* 选择位宽 */
    if (width < 0)
        return;

    long long minV = -(long long)(1ULL << (width - 1));  /* 有符号最小值: -2^(n-1) */
    long long maxV = (long long)((1ULL << (width - 1)) - 1);  /* 有符号最大值: 2^(n-1) - 1 */

    long long v;
    while (1) {                         /* 循环直到输入合法 */
        printf("请输入一个十进制数 (%lld ~ %lld): ", minV, maxV);
        if (!readLongLong(&v)) {        /* 读取整数, 失败则提示 */
            printf("输入错误: 请输入一个合法整数。\n");
            continue;
        }
        if (v < minV || v > maxV) {     /* 超出位宽范围 */
            printf("错误: 超出 %d 位表示范围。\n", width);
            continue;
        }
        break;                          /* 输入合法, 跳出循环 */
    }

    /* 计算补码的无符号表示 */
    uint64_t u = (v < 0) ? ((1ULL << width) - (uint64_t)(-v)) : (uint64_t)v;
    char comp[64];
    toBinaryString(u, width, comp);     /* 转为二进制字符串 */
    printf("\n转换结果: %lld (十进制) = ", v);
    printGrouped(comp);                 /* 分组打印补码 */
    printf(" (二进制补码)\n\n");
    printIntRep(v, width);              /* 打印原码/反码/补码 */
}

/* ================= 浮点数转换功能 (IEEE 754) ================= */

/*
 * choosePrecision: 让用户选择浮点精度
 * 返回: 0=单精度(float, 32位), 1=双精度(double, 64位), -1=EOF
 * 说明: 用户输入 f 或 d (大小写均可), 前后允许空白
 */
static int choosePrecision(void)
{
    char line[64];                      /* 行缓冲区 */
    while (1) {
        printf("请选择精度 (f=单精度32位 / d=双精度64位): ");
        if (!readLine(line, sizeof line))
            return -1;
        char *p = line;
        while (*p && isspace((unsigned char)*p))  /* 跳过前导空白 */
            p++;
        char c = (char)tolower((unsigned char)*p);  /* 取第一个非空字符并转小写 */
        char *q = p + (*p ? 1 : 0);    /* q 指向 c 后面的字符 */
        while (*q && isspace((unsigned char)*q))  /* 跳过 c 后面的空白 */
            q++;
        if ((c == 'f' || c == 'd') && *q == '\0') {  /* 合法: f 或 d, 且后面无其他字符 */
            printf("已选择: %s\n",
                   c == 'f' ? "单精度 float (32位, IEEE 754)"
                            : "双精度 double (64位, IEEE 754)");
            return (c == 'd') ? 1 : 0;  /* d->1(双精度), f->0(单精度) */
        }
        printf("输入无效, 请输入 f 或 d。\n");
    }
}

/*
 * printFloatFields: 拆解并打印 IEEE 754 浮点数的各字段
 * 参数: isDouble - 1=双精度(64位), 0=单精度(32位)
 *       bits - 浮点数的原始位模式 (作为无符号整数传入)
 * 输出:
 *   - 完整二进制 (分组显示)
 *   - S | E | M 三段分离显示
 *   - 十六进制表示
 *   - 符号位、指数位、尾数位的十进制值
 *   - 数值解读 (规格化/非规格化/特殊值)
 */
static void printFloatFields(int isDouble, uint64_t bits)
{
    /* 根据精度确定各字段的位宽和偏移量 */
    int expBits = isDouble ? 11 : 8;    /* 指数位位数: double=11, float=8 */
    int manBits = isDouble ? 52 : 23;   /* 尾数位位数: double=52, float=23 */
    int bias    = isDouble ? 1023 : 127;/* 指数偏移量: double=1023, float=127 */
    uint64_t expMask = (1ULL << expBits) - 1;  /* 指数位掩码, 如 float: 0xFF */

    /* 从位模式中分离三个字段 */
    uint64_t sign = bits >> (expBits + manBits);       /* 符号位: 最高 1 位 */
    uint64_t exp  = (bits >> manBits) & expMask;       /* 指数位: 中间 expBits 位 */
    uint64_t man  = bits & ((1ULL << manBits) - 1);    /* 尾数位: 最低 manBits 位 */

    /* 构造完整的二进制字符串 */
    int total = expBits + manBits + 1;  /* 总位数: 32 或 64 */
    char full[65], expB[12], manB[53]; /* 缓冲区: full最大65, expB最大12, manB最大53 */
    toBinaryString(bits, total, full);  /* 全部位转为 "0101..." 字符串 */
    for (int i = 0; i < expBits; i++)   /* 截取指数位部分 */
        expB[i] = full[1 + i];          /* 从第 2 个字符开始 (跳过符号位) */
    expB[expBits] = '\0';
    for (int i = 0; i < manBits; i++)   /* 截取尾数位部分 */
        manB[i] = full[1 + expBits + i];/* 从符号位+指数位之后开始 */
    manB[manBits] = '\0';

    /* 打印存储结构 */
    printf("\n----- IEEE 754 %s 存储结构 -----\n",
           isDouble ? "双精度(64位)" : "单精度(32位)");
    printf("完整二进制  : ");
    printGrouped(full);                 /* 每 4 位一组打印完整二进制 */
    printf("字段 S-E-M  : %c | %s | %s\n", full[0], expB, manB);  /* 三段分离显示 */
    if (isDouble)
        printf("十六进制    : 0x%016llX\n", (unsigned long long)bits);  /* 64位: 16个十六进制数字 */
    else
        printf("十六进制    : 0x%08llX\n", (unsigned long long)bits);   /* 32位: 8个十六进制数字 */
    printf("符号位 S = %llu (%s)\n", (unsigned long long)sign,
           sign ? "负" : "正");        /* 0=正, 1=负 */
    printf("指数位 E = %s (二进制) = %llu (十进制)\n",
           expB, (unsigned long long)exp);  /* 指数的无偏移原始值 */
    printf("尾数位 M = %s\n", manB);

    /* 数值解读: 根据指数位判断是规格化/非规格化/特殊值 */
    printf("\n----- 数值解读 -----\n");
    if (exp == expMask) {               /* 指数位全 1: 特殊值 */
        if (man == 0)                   /* 尾数全 0 -> 无穷大 */
            printf("特殊值: %s无穷大 (Infinity)\n", sign ? "负" : "正");
        else                            /* 尾数非 0 -> NaN */
            printf("特殊值: 非数 (NaN)\n");
    } else if (exp == 0) {              /* 指数位全 0 */
        if (man == 0)                   /* 尾数也全 0 -> 零 */
            printf("特殊值: %s零\n", sign ? "负" : "正");
        else                            /* 尾数非 0 -> 非规格化数 (次正规数) */
            printf("非规格化数: 值 = (-1)^%llu × 0.%s × 2^%d\n",
                   (unsigned long long)sign, manB, 1 - bias);
        /* 非规格化数: 隐含位为 0 (不是 1), 指数固定为 1-bias (不是 0-bias) */
    } else {                            /* 正常情况: 规格化数 */
        printf("规格化数: 值 = (-1)^%llu × 1.%s × 2^(%llu - %d)\n",
               (unsigned long long)sign, manB,
               (unsigned long long)exp, bias);
        printf("                 = (-1)^%llu × 1.%s × 2^%lld\n",
               (unsigned long long)sign, manB, (long long)exp - bias);
        /* 规格化数: 隐含前导 1, 真实指数 = E - bias */
    }
}

/*
 * floatDecToBin: 功能 3 —— 十进制浮点数 -> 二进制 (IEEE 754)
 * 流程: 选精度(f/d) -> 输入十进制浮点数 -> 输出 IEEE 754 位模式
 * 核心技巧: 用 memcpy 将 float/double 的内存位模式复制为 uint32_t/uint64_t
 *          这是 C99/C11 标准保证合法的"类型双关"(type punning)方式
 */
static void floatDecToBin(void)
{
    int isDouble = choosePrecision();   /* 选择单精度或双精度 */
    if (isDouble < 0)
        return;

    double val;
    while (1) {                         /* 循环直到输入合法 */
        printf("请输入一个十进制浮点数 (如 12.5, -0.1, 1e-3): ");
        if (readDouble(&val))           /* 读取 double */
            break;
        printf("输入错误: 请输入一个合法的浮点数。\n");
    }

    if (isDouble) {                     /* ---- 双精度 (64位) ---- */
        uint64_t bits;
        memcpy(&bits, &val, sizeof bits);  /* 将 double 的 8 字节内存原样复制为 uint64_t */
        printFloatFields(1, bits);      /* 拆解并打印各字段 */
        printf("\n十进制值: %.17g\n", val);  /* double 约 15~17 位有效数字 */
    } else {                            /* ---- 单精度 (32位) ---- */
        float f = (float)val;           /* 将 double 截断为 float */
        /* 精度损失/溢出检测 */
        if (isfinite(val) && isinf((double)f))  /* 有限值转 float 后变无穷 -> 溢出 */
            printf("\n警告: 该值超出单精度范围(约±3.4e38), 将存储为无穷大!\n");
        else if (isfinite(val) && val != 0.0 && f == 0.0f)  /* 非零值变 0 -> 下溢 */
            printf("\n警告: 该值绝对值太小, 单精度下下溢为 0!\n");
        else if (isfinite(val) && (double)f != val)  /* 精度截断 */
            printf("\n提示: 单精度仅约7位有效数字, 实际存储 %.9g (存在精度损失)。\n",
                   (double)f);
        uint32_t b32;
        memcpy(&b32, &f, sizeof b32);   /* 将 float 的 4 字节内存原样复制为 uint32_t */
        printFloatFields(0, b32);       /* 拆解并打印各字段 */
        printf("\n十进制值: %.9g\n", (double)f);  /* float 约 7~9 位有效数字 */
    }
}

/*
 * floatBinToDec: 功能 4 —— 二进制 -> 十进制浮点数 (IEEE 754)
 * 流程: 选精度(f/d) -> 输入 32/64 位二进制串 -> 输出对应的浮点数值
 * 核心技巧: 将二进制串解析为 uint32_t/uint64_t, 再用 memcpy 还原为 float/double
 */
static void floatBinToDec(void)
{
    int isDouble = choosePrecision();   /* 选择精度 */
    if (isDouble < 0)
        return;
    int need = isDouble ? 64 : 32;      /* 需要的二进制位数: 64 或 32 */

    char line[256], bin[80];            /* 行缓冲区和去空格后的二进制串 */
    while (1) {                         /* 循环直到输入合法 */
        printf("请输入 %d 位二进制数 (可用空格分隔): ", need);
        if (!readLine(line, sizeof line))
            return;
        int n = stripSpaces(line, bin, sizeof bin);  /* 去除空格 */
        if (n != need) {                /* 位数不匹配 */
            printf("长度错误: 需要 %d 位, 你输入了 %d 位。\n", need, n);
            continue;
        }
        int ok = 1;
        for (int i = 0; i < n; i++) {   /* 校验每个字符是否为 0 或 1 */
            if (bin[i] != '0' && bin[i] != '1') {
                ok = 0;
                break;
            }
        }
        if (!ok) {
            printf("字符错误: 只能输入 0 和 1。\n");
            continue;
        }
        break;                          /* 校验通过, 跳出循环 */
    }

    /* 将二进制字符串逐位解析为 uint64_t */
    uint64_t bits = 0;
    for (int i = 0; i < need; i++)
        bits = (bits << 1) | (uint64_t)(bin[i] - '0');  /* 左移累加, 与 parseBinary 逻辑相同 */

    printFloatFields(isDouble, bits);   /* 拆解并打印各字段 */

    /* 将位模式还原为浮点数 */
    if (isDouble) {
        double d;
        memcpy(&d, &bits, sizeof d);    /* uint64_t 的 8 字节原样复制为 double */
        printf("\n转换结果: %.17g (十进制 double)\n", d);
    } else {
        uint32_t b32 = (uint32_t)bits;  /* 截取低 32 位 */
        float f;
        memcpy(&f, &b32, sizeof f);     /* uint32_t 的 4 字节原样复制为 float */
        printf("\n转换结果: %.9g (十进制 float)\n", (double)f);
    }
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
    int choice = -1;                    /* 用户选择, 初始化为 -1 确保进入循环 */
    while (choice != 0) {               /* 主循环, 选 0 退出 */
        printf("\n===== 进制转换工具 =====\n");
        printf("  1. 整数: 二进制 -> 十进制\n");
        printf("  2. 整数: 十进制 -> 二进制\n");
        printf("  3. 浮点: 十进制 -> 二进制 (IEEE 754)\n");
        printf("  4. 浮点: 二进制 -> 十进制 (IEEE 754)\n");
        printf("  0. 退出\n");
        printf("========================\n");
        printf("请选择功能 (0-4): ");

        char line[64];
        if (!readLine(line, sizeof line))  /* 读取选择, EOF 则退出 */
            break;
        choice = atoi(line);            /* 将输入转为整数 */

        switch (choice) {               /* 根据选择调用对应功能 */
        case 1: intBinToDec();   break; /* 整数: 二进制 -> 十进制 */
        case 2: intDecToBin();   break; /* 整数: 十进制 -> 二进制 */
        case 3: floatDecToBin(); break; /* 浮点: 十进制 -> 二进制 */
        case 4: floatBinToDec(); break; /* 浮点: 二进制 -> 十进制 */
        case 0: break;                  /* 退出 */
        default:                        /* 无效选择 */
            printf("无效选择, 请重新输入。\n");
            choice = -1;                /* 重置为 -1, 继续循环 */
        }
    }
    printf("已退出。再见!\n");
    return 0;                           /* 正常退出, 返回 0 */
}