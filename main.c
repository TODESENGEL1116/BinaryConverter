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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

/* ================= 通用工具 ================= */

static int readLine(char *buf, size_t size)
{
    if (!fgets(buf, (int)size, stdin))
        return 0;
    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';
    return 1;
}

/* 去除空白, 返回有效长度 */
static int stripSpaces(const char *src, char *dst, size_t dstSize)
{
    size_t n = 0;
    for (const char *p = src; *p && n + 1 < dstSize; p++) {
        if (!isspace((unsigned char)*p))
            dst[n++] = *p;
    }
    dst[n] = '\0';
    return (int)n;
}

/* 校验并解析纯二进制串 -> uint64 */
static int parseBinary(const char *s, uint64_t *out)
{
    if (!*s)
        return 0;
    uint64_t v = 0;
    for (const char *p = s; *p; p++) {
        if (*p != '0' && *p != '1')
            return 0;
        v = (v << 1) | (uint64_t)(*p - '0');
    }
    *out = v;
    return 1;
}

/* 无符号值 -> 指定位宽的二进制串 */
static void toBinaryString(uint64_t val, int bits, char *out)
{
    for (int i = bits - 1; i >= 0; i--)
        out[bits - 1 - i] = ((val >> i) & 1) ? '1' : '0';
    out[bits] = '\0';
}

/* 每4位加空格打印(从高位到低位分组) */
static void printGrouped(const char *s)
{
    int len = (int)strlen(s);
    for (int i = 0; i < len; i++) {
        if (i > 0 && (len - i) % 4 == 0)
            putchar(' ');
        putchar(s[i]);
    }
    putchar('\n');
}

/* 读取 long long (带校验) */
static int readLongLong(long long *out)
{
    char line[256];
    if (!readLine(line, sizeof line))
        return 0;
    errno = 0;
    char *end = NULL;
    long long v = strtoll(line, &end, 10);
    if (end == line || errno == ERANGE)
        return 0;
    while (*end && isspace((unsigned char)*end))
        end++;
    if (*end)
        return 0;
    *out = v;
    return 1;
}

/* 读取 double (允许小数点/科学计数法/inf/nan) */
static int readDouble(double *out)
{
    char line[256];
    if (!readLine(line, sizeof line))
        return 0;
    char *end = NULL;
    double v = strtod(line, &end);
    if (end == line)
        return 0;
    while (*end && isspace((unsigned char)*end))
        end++;
    if (*end)
        return 0;
    *out = v;
    return 1;
}

/* ================= 整数部分 ================= */

static int chooseWidth(void)
{
    char line[64];
    while (1) {
        printf("请选择位宽 (8/16/32): ");
        if (!readLine(line, sizeof line))
            return -1;
        char *end = NULL;
        long w = strtol(line, &end, 10);
        while (*end && isspace((unsigned char)*end))
            end++;
        if (*end == '\0' && (w == 8 || w == 16 || w == 32)) {
            printf("已选择: %ld 位位宽\n", w);
            return (int)w;
        }
        printf("输入无效, 请输入 8、16 或 32。\n");
    }
}

static void printIntRep(long long value, int width)
{
    uint64_t range = 1ULL << width;
    uint64_t u = (value < 0) ? (range - (uint64_t)(-value)) : (uint64_t)value;
    char comp[64];
    toBinaryString(u, width, comp);
    printf("补码: ");
    printGrouped(comp);

    if (value < 0) {
        uint64_t m = (uint64_t)(-value);
        uint64_t maxMag = (range >> 1) - 1;
        if (m <= maxMag) {
            char orig[64], inv[64];
            orig[0] = '1';
            toBinaryString(m, width - 1, orig + 1);
            inv[0] = '1';
            for (int i = 1; i < width; i++)
                inv[i] = (orig[i] == '0') ? '1' : '0';
            inv[width] = '\0';
            printf("原码: ");
            printGrouped(orig);
            printf("反码: ");
            printGrouped(inv);
        } else {
            printf("说明: 该数绝对值为 2^%d, 超出原码/反码表示范围,\n"
                   "      只有补码形式(这是特殊值 %lld 的约定存储)。\n",
                   width - 1, value);
        }
    } else {
        printf("(正数的原码、反码、补码相同)\n");
    }
}

static void intBinToDec(void)
{
    int width = chooseWidth();
    if (width < 0)
        return;

    char line[128];
    int mode = 0;
    while (mode < 1 || mode > 3) {
        printf("解读方式 (1=正数 / 2=负数(绝对值) / 3=补码解读): ");
        if (!readLine(line, sizeof line))
            return;
        mode = atoi(line);
        if (mode < 1 || mode > 3)
            printf("无效选择, 请输入 1、2 或 3。\n");
    }

    printf("请输入一个二进制数: ");
    if (!readLine(line, sizeof line))
        return;
    char bin[128];
    int n = stripSpaces(line, bin, sizeof bin);

    uint64_t u;
    if (n == 0 || n > 63 || !parseBinary(bin, &u)) {
        printf("输入错误: 请输入仅由 0/1 组成的数。\n");
        return;
    }

    if (mode == 1) {
        uint64_t maxMag = (1ULL << (width - 1)) - 1;
        if (u > maxMag) {
            printf("错误: 超出 %d 位正数范围 (最大 %llu)。\n",
                   width, (unsigned long long)maxMag);
            return;
        }
        printf("\n十进制值: +%llu\n\n", (unsigned long long)u);
        printIntRep((long long)u, width);
    } else if (mode == 2) {
        uint64_t maxMag = 1ULL << (width - 1);
        if (u > maxMag) {
            printf("错误: 绝对值超出 %d 位负数范围 (最大绝对值 %llu)。\n",
                   width, (unsigned long long)maxMag);
            return;
        }
        long long value = -(long long)u;
        printf("\n十进制值: %lld\n\n", value);
        if (u == 0)
            printf("说明: 0 的补码为全0 (原码有 +0 和 -0 两种形式)。\n");
        printIntRep(value, width);
    } else {
        if (n != width) {
            printf("错误: 补码解读模式需要恰好 %d 位 (你输入了 %d 位)。\n", width, n);
            return;
        }
        long long value = (u >> (width - 1))
                              ? (long long)(u - (1ULL << width))
                              : (long long)u;
        printf("\n十进制值: %lld\n\n", value);
        printIntRep(value, width);
    }
}

static void intDecToBin(void)
{
    int width = chooseWidth();
    if (width < 0)
        return;

    long long minV = -(long long)(1ULL << (width - 1));
    long long maxV = (long long)((1ULL << (width - 1)) - 1);

    long long v;
    while (1) {
        printf("请输入一个十进制数 (%lld ~ %lld): ", minV, maxV);
        if (!readLongLong(&v)) {
            printf("输入错误: 请输入一个合法整数。\n");
            continue;
        }
        if (v < minV || v > maxV) {
            printf("错误: 超出 %d 位表示范围。\n", width);
            continue;
        }
        break;
    }

    uint64_t u = (v < 0) ? ((1ULL << width) - (uint64_t)(-v)) : (uint64_t)v;
    char comp[64];
    toBinaryString(u, width, comp);
    printf("\n转换结果: %lld (十进制) = ", v);
    printGrouped(comp);
    printf(" (二进制补码)\n\n");
    printIntRep(v, width);
}

/* ================= 浮点部分 (IEEE 754) ================= */

/* 选择精度: 0=单精度float, 1=双精度double, -1=失败 */
static int choosePrecision(void)
{
    char line[64];
    while (1) {
        printf("请选择精度 (f=单精度32位 / d=双精度64位): ");
        if (!readLine(line, sizeof line))
            return -1;
        char *p = line;
        while (*p && isspace((unsigned char)*p))
            p++;
        char c = (char)tolower((unsigned char)*p);
        char *q = p + (*p ? 1 : 0);
        while (*q && isspace((unsigned char)*q))
            q++;
        if ((c == 'f' || c == 'd') && *q == '\0') {
            printf("已选择: %s\n",
                   c == 'f' ? "单精度 float (32位, IEEE 754)"
                            : "双精度 double (64位, IEEE 754)");
            return (c == 'd') ? 1 : 0;
        }
        printf("输入无效, 请输入 f 或 d。\n");
    }
}

/* 拆解并打印 IEEE 754 各字段及数值含义 */
static void printFloatFields(int isDouble, uint64_t bits)
{
    int expBits = isDouble ? 11 : 8;
    int manBits = isDouble ? 52 : 23;
    int bias    = isDouble ? 1023 : 127;
    uint64_t expMask = (1ULL << expBits) - 1;

    uint64_t sign = bits >> (expBits + manBits);
    uint64_t exp  = (bits >> manBits) & expMask;
    uint64_t man  = bits & ((1ULL << manBits) - 1);

    int total = expBits + manBits + 1;
    char full[65], expB[12], manB[53];
    toBinaryString(bits, total, full);
    for (int i = 0; i < expBits; i++)
        expB[i] = full[1 + i];
    expB[expBits] = '\0';
    for (int i = 0; i < manBits; i++)
        manB[i] = full[1 + expBits + i];
    manB[manBits] = '\0';

    printf("\n----- IEEE 754 %s 存储结构 -----\n",
           isDouble ? "双精度(64位)" : "单精度(32位)");
    printf("完整二进制  : ");
    printGrouped(full);
    printf("字段 S-E-M  : %c | %s | %s\n", full[0], expB, manB);
    if (isDouble)
        printf("十六进制    : 0x%016llX\n", (unsigned long long)bits);
    else
        printf("十六进制    : 0x%08llX\n", (unsigned long long)bits);
    printf("符号位 S = %llu (%s)\n", (unsigned long long)sign,
           sign ? "负" : "正");
    printf("指数位 E = %s (二进制) = %llu (十进制)\n",
           expB, (unsigned long long)exp);
    printf("尾数位 M = %s\n", manB);

    printf("\n----- 数值解读 -----\n");
    if (exp == expMask) {
        if (man == 0)
            printf("特殊值: %s无穷大 (Infinity)\n", sign ? "负" : "正");
        else
            printf("特殊值: 非数 (NaN)\n");
    } else if (exp == 0) {
        if (man == 0)
            printf("特殊值: %s零\n", sign ? "负" : "正");
        else
            printf("非规格化数: 值 = (-1)^%llu × 0.%s × 2^%d\n",
                   (unsigned long long)sign, manB, 1 - bias);
    } else {
        printf("规格化数: 值 = (-1)^%llu × 1.%s × 2^(%llu - %d)\n",
               (unsigned long long)sign, manB,
               (unsigned long long)exp, bias);
        printf("                 = (-1)^%llu × 1.%s × 2^%lld\n",
               (unsigned long long)sign, manB, (long long)exp - bias);
    }
}

/* 浮点: 十进制 -> 二进制 */
static void floatDecToBin(void)
{
    int isDouble = choosePrecision();
    if (isDouble < 0)
        return;

    double val;
    while (1) {
        printf("请输入一个十进制浮点数 (如 12.5, -0.1, 1e-3): ");
        if (readDouble(&val))
            break;
        printf("输入错误: 请输入一个合法的浮点数。\n");
    }

    if (isDouble) {
        uint64_t bits;
        memcpy(&bits, &val, sizeof bits);   /* 安全读取位模式 */
        printFloatFields(1, bits);
        printf("\n十进制值: %.17g\n", val);
    } else {
        float f = (float)val;
        if (isfinite(val) && isinf((double)f))
            printf("\n警告: 该值超出单精度范围(约±3.4e38), 将存储为无穷大!\n");
        else if (isfinite(val) && val != 0.0 && f == 0.0f)
            printf("\n警告: 该值绝对值太小, 单精度下下溢为 0!\n");
        else if (isfinite(val) && (double)f != val)
            printf("\n提示: 单精度仅约7位有效数字, 实际存储 %.9g (存在精度损失)。\n",
                   (double)f);
        uint32_t b32;
        memcpy(&b32, &f, sizeof b32);       /* 安全读取位模式 */
        printFloatFields(0, b32);
        printf("\n十进制值: %.9g\n", (double)f);
    }
}

/* 浮点: 二进制 -> 十进制 */
static void floatBinToDec(void)
{
    int isDouble = choosePrecision();
    if (isDouble < 0)
        return;
    int need = isDouble ? 64 : 32;

    char line[256], bin[80];
    while (1) {
        printf("请输入 %d 位二进制数 (可用空格分隔): ", need);
        if (!readLine(line, sizeof line))
            return;
        int n = stripSpaces(line, bin, sizeof bin);
        if (n != need) {
            printf("长度错误: 需要 %d 位, 你输入了 %d 位。\n", need, n);
            continue;
        }
        int ok = 1;
        for (int i = 0; i < n; i++) {
            if (bin[i] != '0' && bin[i] != '1') {
                ok = 0;
                break;
            }
        }
        if (!ok) {
            printf("字符错误: 只能输入 0 和 1。\n");
            continue;
        }
        break;
    }

    uint64_t bits = 0;
    for (int i = 0; i < need; i++)
        bits = (bits << 1) | (uint64_t)(bin[i] - '0');

    printFloatFields(isDouble, bits);

    if (isDouble) {
        double d;
        memcpy(&d, &bits, sizeof d);        /* 安全还原 */
        printf("\n转换结果: %.17g (十进制 double)\n", d);
    } else {
        uint32_t b32 = (uint32_t)bits;
        float f;
        memcpy(&f, &b32, sizeof f);         /* 安全还原 */
        printf("\n转换结果: %.9g (十进制 float)\n", (double)f);
    }
}

/* ================= 主菜单 ================= */

int main(void)
{
    int choice = -1;
    while (choice != 0) {
        printf("\n===== 进制转换工具 =====\n");
        printf("  1. 整数: 二进制 -> 十进制\n");
        printf("  2. 整数: 十进制 -> 二进制\n");
        printf("  3. 浮点: 十进制 -> 二进制 (IEEE 754)\n");
        printf("  4. 浮点: 二进制 -> 十进制 (IEEE 754)\n");
        printf("  0. 退出\n");
        printf("========================\n");
        printf("请选择功能 (0-4): ");

        char line[64];
        if (!readLine(line, sizeof line))
            break;
        choice = atoi(line);

        switch (choice) {
        case 1: intBinToDec();   break;
        case 2: intDecToBin();   break;
        case 3: floatDecToBin(); break;
        case 4: floatBinToDec(); break;
        case 0: break;
        default:
            printf("无效选择, 请重新输入。\n");
            choice = -1;
        }
    }
    printf("已退出。再见!\n");
    return 0;
}