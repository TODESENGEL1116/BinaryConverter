/* * CompleteConverter.c —— 进制转换与编码演示工具 (推导+验算增强版) * * 整数: 二/八/十/十六进制两两转换 (12 项), 支持正数/负数/补码三种解读 * 浮点: IEEE 754 十进制<->二进制/十六进制 (6 项), 支持单/双精度 * * 所有转换均给出逐步推导过程与数值验证 (闭环校验) * 编译: gcc CompleteConverter.c -o CompleteConverter -lm */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <ctype.h>

/* ======================== 通用工具函数 ======================== */

static void myStrRev(char *str)
{
    if (!str) return;
    int len = strlen(str);
    for (int i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = temp;
    }
}

static int readLine(char *buffer, size_t size)
{
    if (fgets(buffer, (int)size, stdin) == NULL)
        return 0;
    size_t len = strlen(buffer);
    while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
        buffer[--len] = '\0';
    }
    return 1;
}

static void stripSpaces(char *str)
{
    char *src = str, *dst = str;
    while (*src) {
        if (!isspace((unsigned char)*src))
            *dst++ = *src;
        src++;
    }
    *dst = '\0';
}

static int chooseWidth(void)
{
    char line[128];
    int width = 0;
    while (1) {
        printf("请选择位宽 (8/16/32/64): ");
        if (!readLine(line, sizeof(line)))
            return -1;
        stripSpaces(line);
        width = atoi(line);
        if (width == 8 || width == 16 || width == 32 || width == 64)
            return width;
        printf("[!] 错误: 请输入 8, 16, 32 或 64。\n");
    }
}

/* ★ 修复核心：独立的解读方式读取函数，使用独立缓冲区，不再污染数字串 */
static int readMode(void)
{
    char mline[32];
    int mode = 0;
    while (mode < 1 || mode > 3) {
        printf("解读方式 (1=正数 / 2=负数(绝对值) / 3=补码解读): ");
        if (!readLine(mline, sizeof(mline)))
            return -1; /* EOF */
        stripSpaces(mline);
        mode = atoi(mline);
        if (mode < 1 || mode > 3)
            printf("[!] 请输入 1、2 或 3。\n");
    }
    return mode;
}

/* ---- 新增: 严格整数解析 (替代裸 atoll, 拒绝 "12ab" / 溢出) ---- */
static int readLongLongStrict(const char *s, long long *out)
{
    if (!*s) return 0;
    errno = 0;
    char *end = NULL;
    long long v = strtoll(s, &end, 10);
    if (end == s || errno == ERANGE) return 0;
    if (*end != '\0') return 0;
    *out = v;
    return 1;
}

/* ---- 新增: 数字字符 <-> 数值 ---- */
static int digitVal(char c, int base)
{
    int v;
    if (c >= '0' && c <= '9') v = c - '0';
    else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
    else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
    else return -1;
    return (v < base) ? v : -1;
}

static char valDigit(int v)
{
    return (v < 10) ? (char)('0' + v) : (char)('A' + v - 10);
}

/* ---- 新增: 除基取余 (无符号版, 可处理 2^63 级别的绝对值) ---- */
static void printDivStepsU(unsigned long long v, int base, char *out, size_t outSize)
{
    char rev[200];
    int rn = 0, shown = 0;
    printf("      除%d取余:\n", base);
    if (v == 0) {
        printf("        0 ÷ %d = 0 余 0\n", base);
        rev[rn++] = '0';
    }
    while (v > 0 && rn < 190) {
        unsigned long long q = v / (unsigned long long)base;
        int r = (int)(v % (unsigned long long)base);
        rev[rn++] = valDigit(r);
        if (shown < 10)
            printf("        %llu ÷ %d = %llu 余 %d\n", v, base, q, r);
        shown++;
        v = q;
    }
    if (shown > 10)
        printf("        ...(其余 %d 步同理)...\n", shown - 10);
    size_t w = 0;                     /* 余数倒序写出 -> 高位在前 */
    while (rn > 0 && w + 1 < outSize)
        out[w++] = rev[--rn];
    out[w] = '\0';
}

/* ---- 新增: 按权展开验算 (base 进制数字串 -> 十进制, 逐项打印) ---- */
static void printWeightExpand(const char *digits, int base, long long expected)
{
    int len = (int)strlen(digits);
    unsigned long long sum = 0;
    int shown = 0;
    printf("      验算 按权展开:\n");
    for (int i = 0; i < len; i++) {
        int d = digitVal(digits[i], base);
        if (d < 0) d = 0;
        unsigned long long wgt = 1;   /* base^(len-1-i), 整数连乘精确 */
        for (int k = 0; k < len - 1 - i; k++) wgt *= (unsigned long long)base;
        sum += (unsigned long long)d * wgt;
        if (d != 0) {
            if (shown < 10)
                printf("        + %d×%d^%d = %llu\n", d, base, len - 1 - i,
                       (unsigned long long)d * wgt);
            shown++;
        }
    }
    if (shown > 10)
        printf("        ...(其余 %d 个非零位同理)...\n", shown - 10);
    printf("        求和 = %llu  (待比对结果 %lld) %s\n", sum, expected,
           (expected >= 0 && sum == (unsigned long long)expected) ? "→ 一致 ✓"
           : (expected < 0 ? "→ 上方为位模式值, 见补码/负号说明" : "→ 不一致!"));
}

/* ---- 新增: 补码权值验算 (符号位权值取负) ---- */
static void verifyTwosComplement(const char *bin, int width, long long expected)
{
    long long chk = 0;
    unsigned long long wgt = 1;
    for (int i = width - 1; i >= 0; i--) {
        unsigned long long bit = (unsigned long long)(bin[i] - '0');
        if (i == 0 && bit) chk -= (long long)(1ULL << (width - 1)); /* 符号位负权 */
        else chk += (long long)(bit * wgt);
        wgt <<= 1;
    }
    printf("      验算 补码按权展开 (符号位权值取负) = %lld", chk);
    printf(chk == expected ? "  → 与结果一致 ✓\n" : "  → 不一致!\n");
}

/* ---- 新增: 负数十进制 -> width 位补码 (原码→反码→+1 逐步演示) ---- */
static void negComplementSteps(long long v, int width, char *out)
{
    unsigned long long umag = 0ULL - (unsigned long long)v; /* 安全求绝对值 */
    char magbin[136];
    printDivStepsU(umag, 2, magbin, sizeof magbin); /* 绝对值除2取余 */
    int mlen = (int)strlen(magbin);
    if (mlen > width - 1) {           /* 只保留低 width-1 位 */
        memmove(magbin, magbin + (mlen - (width - 1)), (size_t)(width - 1));
        magbin[width - 1] = '\0';
        mlen = width - 1;
    }
    char orig[136], inv[136];
    orig[0] = '1';                    /* 原码: 符号位1 + 数值位 (左补0) */
    for (int i = 0; i < width - 1 - mlen; i++) orig[1 + i] = '0';
    strcpy(orig + 1 + (width - 1 - mlen), magbin);
    orig[width] = '\0';
    printf("      原码: %s\n", orig);
    for (int i = 0; i < width; i++)   /* 反码: 符号位不变, 数值位取反 */
        inv[i] = (i == 0) ? '1' : (orig[i] == '0' ? '1' : '0');
    inv[width] = '\0';
    printf("      反码 (数值位取反): %s\n", inv);
    strcpy(out, inv);                 /* 补码 = 反码 + 1 (低位起进位) */
    for (int i = width - 1; i >= 1; i--) {
        if (out[i] == '0') { out[i] = '1'; break; }
        out[i] = '0';
    }
    printf("      反码+1 = 补码: %s\n", out);
}

/* ---- 新增: 二进制串按 k 位一组换算为八/十六进制 (逐步打印分组) ---- */
static void binGroupsToBase(const char *bin, int k, char *out)
{
    int len = (int)strlen(bin);
    int pad = (k - len % k) % k;
    char padded[300];
    int pp = 0;
    for (int i = 0; i < pad && pp < 299; i++) padded[pp++] = '0';
    padded[pp] = '\0';
    strcat(padded, bin);
    int plen = (int)strlen(padded);
    printf("      按 %d 位一组分组 (不足左补0):\n", k);
    int op = 0;
    for (int i = 0; i < plen; i += k) {
        int v = 0;
        for (int j = 0; j < k; j++) v = (v << 1) | (padded[i + j] - '0');
        out[op++] = valDigit(v);
        printf("        %.*s -> %c\n", k, padded + i, valDigit(v));
    }
    out[op] = '\0';
}

/* ---- 新增: 八/十六进制串每位展开为 k 位二进制 (逐步打印) ---- */
static void baseDigitsToBin(const char *digits, int k, int base, char *out)
{
    printf("      每个数字展开为 %d 位二进制:\n", k);
    int op = 0;
    for (int i = 0; digits[i]; i++) {
        int v = digitVal(digits[i], base);
        if (v < 0) v = 0;
        printf("        %c -> ", digits[i]);
        for (int j = k - 1; j >= 0; j--) {
            out[op++] = ((v >> j) & 1) ? '1' : '0';
            putchar(((v >> j) & 1) ? '1' : '0');
        }
        putchar('\n');
    }
    out[op] = '\0';
}

/* ======================== 整数转换基础算法 (原有, 保留) ======================== */

static long long binToLongLong(const char *bin, int width)
{
    long long val = 0;
    int len = strlen(bin);
    for (int i = 0; i < len; i++) {
        val = (val << 1) | (bin[i] - '0');
    }
    if (len > 0 && bin[0] == '1' && width < 64) {
        unsigned long long mask = ~(((1ULL << width) - 1));
        val |= mask;
    }
    return val;
}

static void longLongToBin(long long val, char *bin, size_t size, int width)
{
    (void)size;
    if (width < 64)
        val &= ((1ULL << width) - 1);
    bin[width] = '\0';
    unsigned long long uVal = (unsigned long long)val;
    for (int i = width - 1; i >= 0; i--) {
        bin[i] = (uVal & 1) ? '1' : '0';
        uVal >>= 1;
    }
}

static void longLongToHex(long long val, char *hex, size_t size, int width)
{
    if (width < 64)
        val &= ((1ULL << width) - 1);
    if (width <= 8)
        snprintf(hex, size, "%02X", (unsigned)(val & 0xFF));
    else if (width <= 16)
        snprintf(hex, size, "%04X", (unsigned)(val & 0xFFFF));
    else if (width <= 32)
        snprintf(hex, size, "%08X", (unsigned)(val & 0xFFFFFFFF));
    else
        snprintf(hex, size, "%016llX", (unsigned long long)val);
}

static long long hexToLongLong(const char *hex, int width)
{
    char *end;
    errno = 0;
    unsigned long long uVal = strtoull(hex, &end, 16);
    if (errno == ERANGE || *end != '\0') {
        printf("\n[!] 错误: 数值溢出或包含非法字符。\n");
        return 0;
    }
    if (width < 64) {
        unsigned long long signBitMask = 1ULL << (width - 1);
        if (uVal & signBitMask) {
            uVal |= ~(((1ULL << width) - 1));
        }
    }
    return (long long)uVal;
}

static void octToBinStr(const char *oct, char *bin)
{
    int pos = 0;
    for (int i = 0; oct[i]; i++) {
        int val = oct[i] - '0';
        for (int j = 2; j >= 0; j--) {
            bin[pos++] = ((val >> j) & 1) ? '1' : '0';
        }
    }
    bin[pos] = '\0';
}

// 二进制串 -> 指定位宽的八进制串
static void binStrToOctStr(const char *bin, char *oct, int width)
{
    char paddedBin[136] = {0};
    int binLen = strlen(bin);
    if (binLen < width) {
        memset(paddedBin, '0', width - binLen);
        strcpy(paddedBin + width - binLen, bin);
    } else if (binLen > width) {
        strcpy(paddedBin, bin + binLen - width);
    } else {
        strcpy(paddedBin, bin);
    }
    int binLenPadded = strlen(paddedBin);
    int octPos = 0;
    for (int i = binLenPadded; i > 0; ) {
        int val = 0;
        for (int j = 0; j < 3 && i > 0; j++, i--) {
            val = (val << 1) | (paddedBin[i - 1] - '0');
        }
        oct[octPos++] = val + '0';
    }
    oct[octPos] = '\0';
    myStrRev(oct);
}

static int isHexValid(const char *str)
{
    for (int i = 0; str[i] != '\0'; i++) {
        if (!isxdigit((unsigned char)str[i]))
            return 0;
    }
    return 1;
}

/* ======================== 1. 二进制 <-> 十进制 ======================== */

static void intBinToDec(void)
{
    int width = chooseWidth();
    if (width < 0) return;
    char numStr[256];                     // ★ 独立缓冲区存放二进制串
    printf("请输入二进制串 (可带空格): ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    for (int i = 0; numStr[i]; i++)
        if (numStr[i] != '0' && numStr[i] != '1') {
            printf("[!] 非法二进制。\n");
            return;
        }
    int mode = readMode();                // ★ 不再覆盖 numStr
    if (mode < 0) return;
    long long dec = 0;
    char bits[136] = "";                  /* 新增: 实际参与解读的位模式 (供验算) */
    if (mode == 3) {
        int binLen = strlen(numStr);
        if (binLen < width) {
            char temp[256] = {0};
            memset(temp, '0', width - binLen);
            strcat(temp, numStr);
            dec = binToLongLong(temp, width);
            strcpy(bits, temp);           /* 记录补齐后的位模式 */
        } else {
            int offset = (binLen > width) ? binLen - width : 0;
            dec = binToLongLong(numStr + offset, width);
            memcpy(bits, numStr + offset, (size_t)width);
            bits[width] = '\0';
        }
    } else {
        dec = (mode == 2) ? -binToLongLong(numStr, 64)
                          : binToLongLong(numStr, 64);
        strcpy(bits, numStr);
    }
    printf("\n[+] 十进制结果: %lld\n", dec);

    /* ---- 新增: 数值验证 ---- */
    if (mode == 3) {
        printf("    推导: 补码解读 (位宽 %d, 位模式 %s):\n", width, bits);
        verifyTwosComplement(bits, width, dec);
    } else {
        printf("    推导: %s解读:\n", mode == 2 ? "负数(取绝对值)" : "正数");
        printWeightExpand(bits, 2, mode == 2 ? -dec : dec);
        if (mode == 2 && dec != 0)
            printf("        解读方式为负数 → 结果取 -%llu ✓\n",
                   (unsigned long long)(-dec));
    }
}

static void intDecToBin(void)
{
    int width = chooseWidth();
    if (width < 0) return;
    char line[128];
    long long dec;
    printf("请输入十进制整数: ");
    if (!readLine(line, sizeof(line))) return;
    stripSpaces(line);
    if (!readLongLongStrict(line, &dec)) { /* 新增: 严格校验 */
        printf("[!] 无效整数。\n");
        return;
    }
    char bin[136];
    longLongToBin(dec, bin, sizeof(bin), width);
    printf("\n[+] 二进制结果: %s\n", bin);

    /* ---- 新增: 逐步推导 + 数值验证 ---- */
    printf("    推导:\n");
    if (dec >= 0) {
        char digs[136];
        printDivStepsU((unsigned long long)dec, 2, digs, sizeof digs);
        printf("      余数倒序 = %s, 左补0到 %d 位\n", digs, width);
        printf(strcmp(digs, bin + (width - (int)strlen(digs))) == 0
               ? "      与结果一致 ✓\n" : "      与结果不一致!\n");
    } else {
        char comp[136];
        negComplementSteps(dec, width, comp); /* 原码→反码→+1 */
        printf(strcmp(comp, bin) == 0 ? "      与结果一致 ✓\n" : "      与结果不一致!\n");
    }
    verifyTwosComplement(bin, width, dec); /* 验算: 补码权值展开回到输入值 */
}

/* ======================== 2. 八进制 <-> 十/二进制 ======================== */

static void intOctToDec(void)
{
    int width = chooseWidth();
    if (width < 0) return;
    char numStr[256], bin[1024];          // ★ 独立缓冲区
    printf("请输入八进制串: ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    for (int i = 0; numStr[i]; i++)
        if (numStr[i] < '0' || numStr[i] > '7') {
            printf("[!] 非法八进制。\n");
            return;
        }
    int mode = readMode();                // ★ 不再覆盖 numStr
    if (mode < 0) return;
    octToBinStr(numStr, bin);
    long long dec = 0;
    if (mode == 3) {
        int binLen = strlen(bin);
        if (binLen < width) {
            memmove(bin + (width - binLen), bin, binLen + 1);
            memset(bin, '0', width - binLen);
        }
        int offset = (strlen(bin) > width) ? strlen(bin) - width : 0;
        dec = binToLongLong(bin + offset, width);
    } else if (mode == 2) {
        dec = -strtoll(bin, NULL, 2);
    } else {
        dec = strtoll(bin, NULL, 2);
    }
    printf("\n[+] 十进制结果: %lld\n", dec);

    /* ---- 新增: 推导 (每位展开3位二进制) + 验算 ---- */
    printf("    推导:\n");
    char exp[1024];
    baseDigitsToBin(numStr, 3, 8, exp);
    if (mode == 3)
        printf("      补码解读: 位模式 %s (位宽 %d)\n",
               bin + ((int)strlen(bin) > width ? (int)strlen(bin) - width : 0), width);
    printWeightExpand(numStr, 8, mode == 2 ? -dec : dec);
    if (mode == 3) {
        char padded[136] = {0};
        int bl = (int)strlen(bin);
        memcpy(padded, bin + (bl > width ? bl - width : 0), (size_t)(bl > width ? width : bl));
        verifyTwosComplement(padded, width, dec);
    } else if (mode == 2 && dec != 0)
        printf("        解读方式为负数 → 结果取 %lld ✓\n", dec);
}

static void intDecToOct(void)
{
    int width = chooseWidth();
    if (width < 0) return;
    char line[128];
    long long dec;
    printf("请输入十进制整数: ");
    if (!readLine(line, sizeof(line))) return;
    stripSpaces(line);
    if (!readLongLongStrict(line, &dec)) {
        printf("[!] 无效整数。\n");
        return;
    }
    char bin[136], oct[256];
    longLongToBin(dec, bin, sizeof(bin), width);
    binStrToOctStr(bin, oct, width);
    printf("\n[+] 八进制结果: %s\n", oct);

    /* ---- 新增: 推导 + 验算 ---- */
    printf("    推导:\n");
    if (dec >= 0) {
        char digs[136];
        printDivStepsU((unsigned long long)dec, 8, digs, sizeof digs);
        printf("      余数倒序 = %s\n", digs);
        printf(strcmp(digs, oct + (int)strlen(oct) - (int)strlen(digs)) == 0
               ? "      与结果一致 ✓\n" : "      与结果不一致!\n");
    } else {
        char comp[136], oct2[256];
        negComplementSteps(dec, width, comp); /* 原码→反码→+1 得补码 */
        binGroupsToBase(comp, 3, oct2);       /* 补码按3位一组得八进制 */
        printf(strcmp(oct2, oct) == 0 ? "      与结果一致 ✓\n" : "      与结果不一致!\n");
    }
    printf("    验算:\n");
    printWeightExpand(oct, 8, dec >= 0 ? dec : 0);
    if (dec < 0) {                        /* 负数: 位模式无符号值 - 2^width */
        unsigned long long uval = 0;
        for (int i = 0; oct[i]; i++) uval = uval * 8ULL + (unsigned long long)(oct[i] - '0');
        if (width < 64) {
            long long chk = (long long)(uval - (1ULL << width));
            printf("        补码: 符号位为1 → %llu − 2^%d = %lld", uval, width, chk);
            printf(chk == dec ? "  → 与输入一致 ✓\n" : "  → 不一致!\n");
        } else {
            printf("        64位下负数补码值即为 %lld  → 与输入一致 ✓\n", (long long)uval);
        }
    }
}

/* ======================== 3. 二进制 <-> 八进制 ======================== */

static void intBinToOct(void)
{
    int width = chooseWidth();
    if (width < 0) return;
    char numStr[256];                     // ★ 独立缓冲区
    printf("请输入二进制串 (可带空格): ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    for (int i = 0; numStr[i]; i++)
        if (numStr[i] != '0' && numStr[i] != '1') {
            printf("[!] 非法二进制。\n");
            return;
        }
    int mode = readMode();                // ★ 不再覆盖 numStr
    if (mode < 0) return;
    long long dec = 0;
    if (mode == 3) {
        int binLen = strlen(numStr);
        if (binLen < width) {
            char temp[256] = {0};
            memset(temp, '0', width - binLen);
            strcat(temp, numStr);
            dec = binToLongLong(temp, width);
        } else {
            int offset = (binLen > width) ? binLen - width : 0;
            dec = binToLongLong(numStr + offset, width);
        }
    } else if (mode == 2) {
        dec = -binToLongLong(numStr, 64);
    } else {
        dec = binToLongLong(numStr, 64);
    }
    char bin[136], oct[256];
    longLongToBin(dec, bin, sizeof(bin), width);
    binStrToOctStr(bin, oct, width);
    printf("\n[+] 八进制结果: %s\n", oct);

    /* ---- 新增: 分组法推导 + 验算 ---- */
    printf("    推导 (3位一组直接换算, 无需经过十进制):\n");
    char oct2[256];
    binGroupsToBase(bin, 3, oct2);
    printf(strcmp(oct2, oct) == 0 ? "      组合 %s → 与结果一致 ✓\n" : "      组合 %s → 不一致!\n",
           oct2);
    printf("    验算:\n");
    printWeightExpand(oct, 8, dec >= 0 ? dec : 0);
    if (dec < 0)
        printf("        (结果为 %lld 的补码位模式分组, 负数验证见解读方式)\n", dec);
}

static void intOctToBin(void)
{
    int width = chooseWidth();
    if (width < 0) return;
    char numStr[256], bin[1024];          // ★ 独立缓冲区
    printf("请输入八进制串: ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    for (int i = 0; numStr[i]; i++)
        if (numStr[i] < '0' || numStr[i] > '7') {
            printf("[!] 非法八进制。\n");
            return;
        }
    int mode = readMode();                // ★ 不再覆盖 numStr
    if (mode < 0) return;
    octToBinStr(numStr, bin);
    long long dec = 0;
    if (mode == 3) {
        int binLen = strlen(bin);
        if (binLen < width) {
            memmove(bin + (width - binLen), bin, binLen + 1);
            memset(bin, '0', width - binLen);
        }
        int offset = (strlen(bin) > width) ? strlen(bin) - width : 0;
        dec = binToLongLong(bin + offset, width);
    } else if (mode == 2) {
        dec = -strtoll(bin, NULL, 2);
    } else {
        dec = strtoll(bin, NULL, 2);
    }
    char outBin[136];
    longLongToBin(dec, outBin, sizeof(outBin), width);
    printf("\n[+] 二进制结果: %s\n", outBin);

    /* ---- 新增: 逐位展开推导 + 验算 ---- */
    printf("    推导:\n");
    char exp[1024];
    baseDigitsToBin(numStr, 3, 8, exp);   /* 每位八进制 -> 3位二进制 */
    char padded[136] = {0};
    int el = (int)strlen(exp);
    if (el < width) { memset(padded, '0', width - el); strcpy(padded + (width - el), exp); }
    else strcpy(padded, exp + (el > width ? el - width : 0));
    printf(strcmp(padded, outBin) == 0 ? "      补齐到 %d 位: %s → 与结果一致 ✓\n"
                                       : "      补齐到 %d 位: %s → 不一致!\n",
           width, padded);
    printf("    验算:\n");
    printWeightExpand(outBin, 2, dec >= 0 ? dec : 0);
    if (mode == 3 || dec < 0)
        printf("        (补码解读时上式为位模式无符号值)\n");
}

/* ======================== 4. 十六进制相关 ======================== */

static void intHexToDec(void)
{
    int width = chooseWidth();
    if (width < 0) return;
    char numStr[256];                     // ★ 独立缓冲区
    printf("请输入十六进制串: ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    if (!isHexValid(numStr)) {
        printf("[!] 非法十六进制。\n");
        return;
    }
    int mode = readMode();                // ★ 不再覆盖 numStr
    if (mode < 0) return;
    int hexLen = strlen(numStr);
    int targetHexLen = width / 4;
    if (hexLen > targetHexLen) {
        printf("[提示] 输入长度超过位宽，已截取低 %d 位。\n", width);
        memmove(numStr, numStr + hexLen - targetHexLen, targetHexLen + 1);
        hexLen = targetHexLen;
    }
    if (mode == 3 && hexLen < targetHexLen) {
        char temp[256] = {0};
        memset(temp, '0', targetHexLen - hexLen);
        strcat(temp, numStr);
        strcpy(numStr, temp);
    }
    long long dec = hexToLongLong(numStr, width);
    if (mode == 2) dec = -strtoull(numStr, NULL, 16);
    printf("\n[+] 十进制结果: %lld\n", dec);

    /* ---- 新增: 推导 + 验算 ---- */
    if (mode == 3)
        printf("    推导: 补码解读 (位宽 %d, 位模式 %s):\n", width, numStr);
    else
        printf("    推导:\n");
    printWeightExpand(numStr, 16, mode == 2 ? -dec : dec);
    if (mode == 3) {
        char bin[136];
        longLongToBin(dec, bin, sizeof(bin), width);
        verifyTwosComplement(bin, width, dec);
    } else if (mode == 2 && dec != 0)
        printf("        解读方式为负数 → 结果取 %lld ✓\n", dec);
}

static void intDecToHex(void)
{
    int width = chooseWidth();
    if (width < 0) return;
    char line[128];
    long long dec;
    printf("请输入十进制整数: ");
    if (!readLine(line, sizeof(line))) return;
    stripSpaces(line);
    if (!readLongLongStrict(line, &dec)) {
        printf("[!] 无效整数。\n");
        return;
    }
    char hex[136];
    longLongToHex(dec, hex, sizeof(hex), width);
    printf("\n[+] 十六进制结果: %s\n", hex);

    /* ---- 新增: 推导 + 验算 ---- */
    printf("    推导:\n");
    if (dec >= 0) {
        char digs[136];
        printDivStepsU((unsigned long long)dec, 16, digs, sizeof digs);
        printf("      余数倒序 = %s\n", digs);
        printf(strcmp(digs, hex + (int)strlen(hex) - (int)strlen(digs)) == 0
               ? "      与结果一致 ✓\n" : "      与结果不一致!\n");
    } else {
        char comp[136], hex2[136];
        negComplementSteps(dec, width, comp); /* 原码→反码→+1 */
        binGroupsToBase(comp, 4, hex2);       /* 补码按4位一组得十六进制 */
        printf(strcmp(hex2, hex) == 0 ? "      与结果一致 ✓\n" : "      与结果不一致!\n");
    }
    printf("    验算:\n");
    printWeightExpand(hex, 16, dec >= 0 ? dec : 0);
    if (dec < 0) {
        unsigned long long uval = 0;
        for (int i = 0; hex[i]; i++) uval = uval * 16ULL + (unsigned long long)digitVal(hex[i], 16);
        if (width < 64) {
            long long chk = (long long)(uval - (1ULL << width));
            printf("        补码: 符号位为1 → %llu − 2^%d = %lld", uval, width, chk);
            printf(chk == dec ? "  → 与输入一致 ✓\n" : "  → 不一致!\n");
        } else {
            printf("        64位下负数补码值即为 %lld  → 与输入一致 ✓\n", (long long)uval);
        }
    }
}

static void intBinToHex(void)
{
    int width = chooseWidth();
    if (width < 0) return;
    char numStr[256];                     // ★ 独立缓冲区
    printf("请输入二进制串 (可带空格): ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    for (int i = 0; numStr[i]; i++)
        if (numStr[i] != '0' && numStr[i] != '1') {
            printf("[!] 非法二进制。\n");
            return;
        }
    int mode = readMode();                // ★ 不再覆盖 numStr
    if (mode < 0) return;
    long long dec = 0;
    if (mode == 3) {
        int binLen = strlen(numStr);
        if (binLen < width) {
            char temp[256] = {0};
            memset(temp, '0', width - binLen);
            strcat(temp, numStr);
            dec = binToLongLong(temp, width);
        } else {
            int offset = (binLen > width) ? binLen - width : 0;
            dec = binToLongLong(numStr + offset, width);
        }
    } else if (mode == 2) {
        dec = -binToLongLong(numStr, 64);
    } else {
        dec = binToLongLong(numStr, 64);
    }
    char hex[136];
    longLongToHex(dec, hex, sizeof(hex), width);
    printf("\n[+] 十六进制结果: %s\n", hex);

    /* ---- 新增: 分组法推导 + 验算 ---- */
    printf("    推导 (4位一组直接换算, 无需经过十进制):\n");
    char bin[136], hex2[136];
    longLongToBin(dec, bin, sizeof(bin), width);
    binGroupsToBase(bin, 4, hex2);
    printf(strcmp(hex2, hex) == 0 ? "      组合 %s → 与结果一致 ✓\n" : "      组合 %s → 不一致!\n",
           hex2);
    printf("    验算:\n");
    printWeightExpand(hex, 16, dec >= 0 ? dec : 0);
    if (dec < 0)
        printf("        (结果为 %lld 的补码位模式分组)\n", dec);
}

static void intHexToBin(void)
{
    int width = chooseWidth();
    if (width < 0) return;
    char numStr[256];                     // ★ 独立缓冲区
    printf("请输入十六进制串: ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    if (!isHexValid(numStr)) {
        printf("[!] 非法十六进制。\n");
        return;
    }
    int mode = readMode();                // ★ 不再覆盖 numStr
    if (mode < 0) return;
    int hexLen = strlen(numStr);
    int targetHexLen = width / 4;
    if (hexLen > targetHexLen) {
        printf("[提示] 输入长度超过位宽，已截取低 %d 位。\n", width);
        memmove(numStr, numStr + hexLen - targetHexLen, targetHexLen + 1);
        hexLen = targetHexLen;
    }
    if (mode == 3 && hexLen < targetHexLen) {
        char temp[256] = {0};
        memset(temp, '0', targetHexLen - hexLen);
        strcat(temp, numStr);
        strcpy(numStr, temp);
    }
    long long dec = hexToLongLong(numStr, width);
    if (mode == 2) dec = -strtoull(numStr, NULL, 16);
    char bin[136];
    longLongToBin(dec, bin, sizeof(bin), width);
    printf("\n[+] 二进制结果: %s\n", bin);

    /* ---- 新增: 逐位展开推导 + 验算 ---- */
    printf("    推导:\n");
    char exp[1024];
    baseDigitsToBin(numStr, 4, 16, exp);  /* 每个 hex 数字 -> 4位二进制 */
    char padded[136] = {0};
    int el = (int)strlen(exp);
    if (el < width) { memset(padded, '0', width - el); strcpy(padded + (width - el), exp); }
    else strcpy(padded, exp + (el > width ? el - width : 0));
    printf(strcmp(padded, bin) == 0 ? "      补齐到 %d 位: %s → 与结果一致 ✓\n"
                                    : "      补齐到 %d 位: %s → 不一致!\n",
           width, padded);
    printf("    验算:\n");
    printWeightExpand(bin, 2, dec >= 0 ? dec : 0);
    if (mode == 3 || dec < 0)
        printf("        (补码解读时上式为位模式无符号值)\n");
}

static void intOctToHex(void)
{
    int width = chooseWidth();
    if (width < 0) return;
    char numStr[256], bin[1024];          // ★ 独立缓冲区
    printf("请输入八进制串: ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    for (int i = 0; numStr[i]; i++)
        if (numStr[i] < '0' || numStr[i] > '7') {
            printf("[!] 非法八进制。\n");
            return;
        }
    int mode = readMode();                // ★ 不再覆盖 numStr
    if (mode < 0) return;
    octToBinStr(numStr, bin);
    long long dec = 0;
    if (mode == 3) {
        int binLen = strlen(bin);
        if (binLen < width) {
            memmove(bin + (width - binLen), bin, binLen + 1);
            memset(bin, '0', width - binLen);
        }
        int offset = (strlen(bin) > width) ? strlen(bin) - width : 0;
        dec = binToLongLong(bin + offset, width);
    } else if (mode == 2) {
        dec = -strtoll(bin, NULL, 2);
    } else {
        dec = strtoll(bin, NULL, 2);
    }
    char hex[136];
    longLongToHex(dec, hex, sizeof(hex), width);
    printf("\n[+] 十六进制结果: %s\n", hex);

    /* ---- 新增: 两段分组推导 (八→二→十六) + 验算 ---- */
    printf("    推导 (经二进制中转: 每位八进制=3位二进制, 再按4位一组): \n");
    char mid[1024], hex2[136];
    baseDigitsToBin(numStr, 3, 8, mid);
    char padded[136] = {0};
    int ml = (int)strlen(mid);
    if (ml < width) { memset(padded, '0', width - ml); strcpy(padded + (width - ml), mid); }
    else strcpy(padded, mid + (ml > width ? ml - width : 0));
    binGroupsToBase(padded, 4, hex2);
    printf(strcmp(hex2, hex) == 0 ? "      组合 %s → 与结果一致 ✓\n" : "      组合 %s → 不一致!\n",
           hex2);
    printf("    验算:\n");
    printWeightExpand(hex, 16, dec >= 0 ? dec : 0);
    if (dec < 0)
        printf("        (结果为 %lld 的补码位模式分组)\n", dec);
}

static void intHexToOct(void)
{
    int width = chooseWidth();
    if (width < 0) return;
    char numStr[256];                     // ★ 独立缓冲区
    printf("请输入十六进制串: ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    if (!isHexValid(numStr)) {
        printf("[!] 非法十六进制。\n");
        return;
    }
    int mode = readMode();                // ★ 不再覆盖 numStr
    if (mode < 0) return;
    int hexLen = strlen(numStr);
    int targetHexLen = width / 4;
    if (hexLen > targetHexLen) {
        printf("[提示] 输入长度超过位宽，已截取低 %d 位。\n", width);
        memmove(numStr, numStr + hexLen - targetHexLen, targetHexLen + 1);
        hexLen = targetHexLen;
    }
    if (mode == 3 && hexLen < targetHexLen) {
        char temp[256] = {0};
        memset(temp, '0', targetHexLen - hexLen);
        strcat(temp, numStr);
        strcpy(numStr, temp);
    }
    long long dec = hexToLongLong(numStr, width);
    if (mode == 2) dec = -strtoull(numStr, NULL, 16);
    char bin[136], oct[256];
    longLongToBin(dec, bin, sizeof(bin), width);
    binStrToOctStr(bin, oct, width);
    printf("\n[+] 八进制结果: %s\n", oct);

    /* ---- 新增: 两段分组推导 (十六→二→八) + 验算 ---- */
    printf("    推导 (经二进制中转: 每位十六进制=4位二进制, 再按3位一组): \n");
    char mid[1024], oct2[256];
    baseDigitsToBin(numStr, 4, 16, mid);
    char padded[136] = {0};
    int ml = (int)strlen(mid);
    if (ml < width) { memset(padded, '0', width - ml); strcpy(padded + (width - ml), mid); }
    else strcpy(padded, mid + (ml > width ? ml - width : 0));
    binGroupsToBase(padded, 3, oct2);
    printf(strcmp(oct2, oct) == 0 ? "      组合 %s → 与结果一致 ✓\n" : "      组合 %s → 不一致!\n",
           oct2);
    printf("    验算:\n");
    printWeightExpand(oct, 8, dec >= 0 ? dec : 0);
    if (dec < 0)
        printf("        (结果为 %lld 的补码位模式分组)\n", dec);
}

/* ======================== 5. IEEE 754 浮点转换 (增强版) ======================== */

/* ---- 新增: 无符号整数 -> 二进制串 / 分组打印 ---- */
static void toBinaryString(uint64_t val, int bits, char *out)
{
    for (int i = bits - 1; i >= 0; i--)
        out[bits - 1 - i] = ((val >> i) & 1) ? '1' : '0';
    out[bits] = '\0';
}

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

/* ---- 新增: "×10^"/"\times" 写法归一化 + 严格 double 解析 ---- */
static void normalizeSciInput(const char *src, char *dst, size_t dstSize)
{
    size_t n = 0;
    for (const char *p = src; *p && n + 1 < dstSize; p++) {
        if (strncmp(p, "\\times", 6) == 0) {
            dst[n++] = 'e';
            p += 5;
        } else if (*p == '×' || *p == 'x' || *p == 'X') {
            dst[n++] = 'e';
        } else if (*p == '^') {
            const char *q = p + 1;
            if (*q == '{') {
                q++;
                while (*q && *q != '}' && n + 1 < dstSize)
                    dst[n++] = *q++;
                if (*q == '}') p = q;
            }
        } else if (*p == '{' || *p == '}') {
            continue;
        } else {
            dst[n++] = *p;
        }
    }
    dst[n] = '\0';
}

static int readDoubleStrict(const char *s, double *out)
{
    char norm[256];
    normalizeSciInput(s, norm, sizeof norm);
    char *end = NULL;
    errno = 0;
    double v = strtod(norm, &end);
    if (end == norm || errno == ERANGE) return 0;
    if (*end != '\0') return 0;
    *out = v;
    return 1;
}

static int choosePrecision(void)
{
    char line[64];
    while (1) {
        printf("请选择精度 (f=单精度32位 / d=双精度64位): ");
        if (!readLine(line, sizeof(line))) return -1;
        stripSpaces(line);
        char c = (char)tolower((unsigned char)line[0]);
        if ((c == 'f' || c == 'd') && line[1] == '\0')
            return (c == 'd') ? 1 : 0;
        printf("[!] 请输入 f 或 d。\n");
    }
}

/*
 * readFloatBits: 读取浮点位模式 —— 二进制/十六进制自动识别
 * ★ 修复原 bug: 先剥掉 0x/0X 前缀再校验长度, "0x411A0000" 不再被误拒
 * 返回: 成功 1, 失败/EOF 0
 */
static int readFloatBits(int isDouble, uint64_t *bits)
{
    int need = isDouble ? 64 : 32;
    int needHex = need / 4;
    char line[256];
    while (1) {
        printf("请输入 %d 位二进制数 或 %d 位十六进制数 (可带空格, 十六进制可带 0x 前缀): ",
               need, needHex);
        if (!readLine(line, sizeof(line))) return 0;
        stripSpaces(line);
        int n = (int)strlen(line);
        if (n == 0) { printf("[!] 输入为空。\n"); continue; }
        /* ★ 修复: 0x/0X 前缀 -> 剥掉后按纯 hex 长度校验 */
        int isHex = (line[0] == '0' && (line[1] == 'x' || line[1] == 'X'));
        if (isHex) {
            memmove(line, line + 2, (size_t)n - 1); /* 前移覆盖前缀 (含'\0') */
            n -= 2;
        }
        int only01 = 1, onlyHexCh = 1;
        for (int i = 0; i < n; i++) {
            char c = (char)tolower((unsigned char)line[i]);
            if (line[i] != '0' && line[i] != '1') only01 = 0;
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) onlyHexCh = 0;
        }
        if (isHex || (!only01 && onlyHexCh)) {
            if (n != needHex) { printf("[!] 十六进制需要 %d 位。\n", needHex); continue; }
            if (!onlyHexCh) { printf("[!] 十六进制只能含 0-9/A-F/a-f。\n"); continue; }
            unsigned long long uv = 0;
            for (int i = 0; i < n; i++) uv = (uv << 4) | (unsigned long long)digitVal(line[i], 16);
            *bits = (uint64_t)uv;
            printf("已识别为十六进制输入。\n");
        } else if (only01) {
            if (n != need) { printf("[!] 二进制需要 %d 位。\n", need); continue; }
            uint64_t b = 0;
            for (int i = 0; i < n; i++) b = (b << 1) | (uint64_t)(line[i] - '0');
            *bits = b;
            printf("已识别为二进制输入。\n");
        } else {
            printf("[!] 输入错误: 只能是二进制或十六进制。\n");
            continue;
        }
        return 1;
    }
}

/* ---- 新增: 编码逐步推导 (十进制 -> IEEE 754) ---- */
static void printEncodingSteps(int isDouble, uint64_t bits, double val)
{
    int expBits = isDouble ? 11 : 8;
    int manBits = isDouble ? 52 : 23;
    int bias = isDouble ? 1023 : 127;
    uint64_t expMask = (1ULL << expBits) - 1;
    uint64_t sign = bits >> (expBits + manBits);
    uint64_t exp = (bits >> manBits) & expMask;
    uint64_t man = bits & ((1ULL << manBits) - 1);
    long long realExp = (long long)exp - bias;
    char expB[12], manB[53];
    toBinaryString(exp, expBits, expB);
    toBinaryString(man, manBits, manB);

    printf("    推导:\n");
    if (exp == expMask || exp == 0) {     /* 特殊值不适用常规五步 */
        printf("      第1步 原值: %.17g\n", val);
        printf("      说明: 特殊编码(零/非规格化/无穷大/NaN), 按 IEEE 754 约定直接编码。\n");
        return;
    }
    printf("      第1步 原值: %.17g\n", val);
    double fracVal = 1.0 + (double)man / (double)(1ULL << manBits);
    printf("      第2步 规格化: = %s%.17g × 2^%lld\n", sign ? "-" : "", fracVal, realExp);
    printf("      第3步 指数域: %lld + %d = %llu = %s (%d 位)\n",
           realExp, bias, (unsigned long long)exp, expB, expBits);
    double f = (double)man / (double)(1ULL << manBits); /* 小数部分 0.M, 2的幂运算精确 */
    printf("      第4步 尾数域: 对 %.10g 用\"乘2取整法\":\n", f);
    int show = (manBits < 8) ? manBits : 8;
    for (int i = 0; i < show; i++) {
        double prev = f;
        f *= 2;
        int bit = (f >= 1.0);
        if (bit) f -= 1.0;
        printf("        %d) %.10g × 2 = %.10g → 取 %d, 余 %.10g\n", i + 1, prev, prev * 2, bit, f);
    }
    if (manBits > show)
        printf("        ...(其余 %d 位同理)...\n", manBits - show);
    printf("      第5步 拼接: %llu | %s | %s\n", (unsigned long long)sign, expB, manB);
}

/* ---- 新增: 解码逐步推导 (IEEE 754 -> 十进制) ---- */
static void printDecodingSteps(int isDouble, uint64_t bits)
{
    int expBits = isDouble ? 11 : 8;
    int manBits = isDouble ? 52 : 23;
    int bias = isDouble ? 1023 : 127;
    uint64_t expMask = (1ULL << expBits) - 1;
    uint64_t sign = bits >> (expBits + manBits);
    uint64_t exp = (bits >> manBits) & expMask;
    uint64_t man = bits & ((1ULL << manBits) - 1);
    char expB[12], manB[53];
    toBinaryString(exp, expBits, expB);
    toBinaryString(man, manBits, manB);

    printf("    推导:\n");
    printf("      第1步 拆分: S|E|M = %llu | %s | %s\n", (unsigned long long)sign, expB, manB);
    if (exp == expMask || exp == 0) {
        printf("      说明: 特殊编码(零/非规格化/无穷大/NaN), 按专门规则解读。\n");
        return;
    }
    printf("      第2步 符号: S=%llu → %s\n", (unsigned long long)sign, sign ? "负" : "正");
    long long realExp = (long long)exp - bias;
    double p2 = pow(2.0, (double)realExp);
    printf("      第3步 指数: %llu − %d = %lld,  2^%lld = %.10g\n",
           (unsigned long long)exp, bias, realExp, realExp, p2);
    printf("      第4步 尾数: 隐含位1 + 为1的位按权展开:\n");
    double frac = 1.0;
    int shown = 0;
    for (int i = 0; i < manBits; i++) {
        if (manB[i] == '1') {
            double w = pow(2.0, (double)(-(i + 1)));
            frac += w;
            if (shown < 8)
                printf("        第%d位=1 → 2^-%d = %.10g\n", i + 1, i + 1, w);
            shown++;
        }
    }
    if (shown > 8)
        printf("        ...(其余 %d 个为1的位同理)...\n", shown - 8);
    printf("        求和 1.M = %.10g\n", frac);
    printf("      第5步 组装: %s%.10g × %.10g = %s%.10g\n",
           sign ? "-" : "", frac, p2, sign ? "-" : "", frac * p2);
}

/*
 * printFloatFields: 存储结构 + 数值验证
 * userInputIsDec=1: 从二进制按权展开验算回十进制 (输入是十进制时)
 * userInputIsDec=0: 从十进制除2取余/乘2取整反推回二进制 (输入是二/十六进制时)
 */
static void printFloatFields(int isDouble, uint64_t bits, int userInputIsDec, double decVal)
{
    int expBits = isDouble ? 11 : 8;
    int manBits = isDouble ? 52 : 23;
    int bias = isDouble ? 1023 : 127;
    uint64_t expMask = (1ULL << expBits) - 1;
    uint64_t sign = bits >> (expBits + manBits);
    uint64_t exp = (bits >> manBits) & expMask;
    uint64_t man = bits & ((1ULL << manBits) - 1);
    int total = expBits + manBits + 1;
    char full[65], expB[12], manB[53];
    toBinaryString(bits, total, full);
    for (int i = 0; i < expBits; i++) expB[i] = full[1 + i];
    expB[expBits] = '\0';
    for (int i = 0; i < manBits; i++) manB[i] = full[1 + expBits + i];
    manB[manBits] = '\0';

    printf("    存储结构: ");
    printGrouped(full);
    printf("            S=%llu E=%s(%llu) M=%s\n",
           (unsigned long long)sign, expB, (unsigned long long)exp, manB);
    if (isDouble) printf("            十六进制: 0x%016llX\n", (unsigned long long)bits);
    else          printf("            十六进制: 0x%08llX\n", (unsigned long long)bits);

    printf("    验证:\n");
    if (exp == expMask) {
        if (man == 0) printf("      特殊值: %s无穷大\n", sign ? "负" : "");
        else          printf("      特殊值: NaN\n");
        return;
    }
    if (exp == 0) {
        if (man == 0) printf("      特殊值: %s零\n", sign ? "负" : "");
        else printf("      非规格化数: (-1)^%llu × 0.%s × 2^%d\n",
                    (unsigned long long)sign, manB, 1 - bias);
        return;
    }
    long long realExp = (long long)exp - bias;

    /* 生成移位后的二进制串 (最简形式), 缓冲区 1200 防极端指数溢出 */
    char digits[80], trimmed[1200];
    digits[0] = '1';
    memcpy(digits + 1, manB, (size_t)manBits);
    int digitsLen = manBits + 1;
    int t = 0;
    if (realExp >= 0) {
        int pt = 1 + (int)realExp;
        for (int i = 0; i < digitsLen; i++) {
            if (i == pt) trimmed[t++] = '.';
            trimmed[t++] = digits[i];
        }
        for (int i = digitsLen; i < pt; i++) trimmed[t++] = '0';
        if (digitsLen <= pt) { trimmed[t++] = '.'; trimmed[t++] = '0'; }
    } else {
        trimmed[t++] = '0'; trimmed[t++] = '.';
        for (long long i = 0; i < -realExp - 1; i++) trimmed[t++] = '0';
        for (int i = 0; i < digitsLen; i++) trimmed[t++] = digits[i];
    }
    trimmed[t] = '\0';
    while (t > 0 && trimmed[t - 1] == '0') trimmed[--t] = '\0';
    if (t > 0 && trimmed[t - 1] == '.') trimmed[--t] = '\0';
    printf("      二进制还原: (%s)(二进制)  最简形式: (%s)\n", trimmed, trimmed);

    double frac = 1.0, w = 0.5;
    for (int i = 0; i < manBits; i++, w /= 2)
        if (manB[i] == '1') frac += w;
    double v = (sign ? -1.0 : 1.0) * frac * pow(2.0, (double)realExp);

    if (userInputIsDec) {
        /* ---- 输入是十进制: 二进制按权展开验算回十进制 ---- */
        printf("      二进制→十进制, 按权展开:\n");
        double sum = 0.0;
        int terms = 0;
        char *dot = strchr(trimmed, '.');
        int intLen = dot ? (int)(dot - trimmed) : (int)strlen(trimmed);
        for (int i = 0; i < intLen; i++) {
            if (trimmed[i] != '1') continue;
            double wgt = pow(2.0, (double)(intLen - 1 - i));
            sum += wgt;
            if (terms < 8) printf("            + 1×2^%d = %.10g\n", intLen - 1 - i, wgt);
            terms++;
        }
        if (dot) {
            int flen = (int)strlen(dot + 1);
            for (int j = 0; j < flen; j++) {
                if (dot[1 + j] != '1') continue;
                double wgt = pow(2.0, -(double)(j + 1));
                sum += wgt;
                if (terms < 8) printf("            + 1×2^-%d = %.10g\n", j + 1, wgt);
                terms++;
            }
        }
        if (terms > 8) printf("            ...(其余 %d 项同理)...\n", terms - 8);
        if (sign) printf("            符号位 S=1 → 取负\n");
        double tot = sign ? -sum : sum;
        printf("            求和 = %.10g", tot);
        printf(tot == decVal ? "  → 与输入的十进制值 %.10g 一致 ✓\n" : "  → 存在差异!\n", decVal);
    } else {
        /* ---- 输入是二/十六进制: 十进制结果反推回二进制 ---- */
        printf("      十进制→二进制, 反向验算:\n");
        double av = fabs(decVal);
        double ip = floor(av), fp = av - ip;
        char rec[1200];
        int t2 = 0;
        if (ip == 0.0) {
            printf("            整数部分 0 → 0\n");
            rec[t2++] = '0';
        } else {
            char rev[1200];
            int rn = 0, shown = 0;
            double n = ip;
            printf("            整数部分 %.0f: 除2取余\n", ip);
            while (n >= 1.0 && rn < 1100) {
                double half = n / 2.0;
                double fl = floor(half);
                int r = (int)(n - 2.0 * fl);
                rev[rn++] = (char)('0' + r);
                if (shown < 8) printf("              %.0f ÷ 2 = %.0f 余 %d\n", n, fl, r);
                shown++;
                n = fl;
            }
            if (shown > 8) printf("              ...(其余 %d 步同理)...\n", shown - 8);
            while (rn > 0) rec[t2++] = rev[--rn];
        }
        rec[t2++] = '.';
        if (fp == 0.0) {
            printf("            小数部分 0 → 无小数位\n");
            rec[t2++] = '0';
        } else {
            printf("            小数部分 %.10g: 乘2取整\n", fp);
            int shown = 0;
            while (fp > 0.0 && t2 < 1190) {
                double prev = fp;
                double d2 = fp * 2.0;
                int bit = (d2 >= 1.0);
                fp = d2 - (double)bit;
                rec[t2++] = (char)('0' + bit);
                if (shown < 8)
                    printf("              %.10g × 2 = %.10g → 取 %d, 余 %.10g\n",
                           prev, prev * 2.0, bit, fp);
                shown++;
            }
            if (shown > 8) printf("              ...(其余 %d 步同理)...\n", shown - 8);
        }
        rec[t2] = '\0';
        while (t2 > 0 && rec[t2 - 1] == '0') rec[--t2] = '\0';
        if (t2 > 0 && rec[t2 - 1] == '.') rec[--t2] = '\0';
        printf("            组合: %s%s\n", sign ? "-" : "", rec);
        if (strcmp(rec, trimmed) == 0)
            printf("            与最简形式 (%s) 一致 → 反推回位模式 ✓\n", trimmed);
        else
            printf("            与最简形式 (%s) 不一致, 请检查!\n", trimmed);
    }
}

/* ---- 功能 13/18: 十进制 -> 二进制/十六进制 (共用流水线) ---- */
static void floatDecToConv(int wantHex)
{
    int isDouble = choosePrecision();
    if (isDouble < 0) return;
    char line[256];
    double val;
    printf("请输入十进制浮点数 (支持科学计数法: 12.5, -0.1, 1e-3, 1.5×10^3): ");
    if (!readLine(line, sizeof(line))) return;
    stripSpaces(line);
    if (!readDoubleStrict(line, &val)) {
        printf("[!] 无效浮点数。\n");
        return;
    }
    if (isDouble) {
        uint64_t bits;
        memcpy(&bits, &val, sizeof bits);
        printf("\n[+] IEEE 754 双精度(64位):\n");
        printEncodingSteps(1, bits, val);
        printFloatFields(1, bits, 1, val);
        char binB[80];
        toBinaryString(bits, 64, binB);
        if (wantHex)
            printf("\n[+] 结果: 0x%016llX (十六进制) = %.17g (十进制)\n",
                   (unsigned long long)bits, val);
        else {
            printf("\n[+] 结果: ");
            printGrouped(binB);
            printf("    十六进制: 0x%016llX\n", (unsigned long long)bits);
        }
    } else {
        float f = (float)val;
        if (isfinite(val) && isinf((double)f))
            printf("[!] 警告: 超出单精度范围, 将存储为无穷大!\n");
        else if (isfinite(val) && val != 0.0 && f == 0.0f)
            printf("[!] 警告: 绝对值太小, 单精度下下溢为 0!\n");
        uint32_t b32;
        memcpy(&b32, &f, sizeof b32);
        printf("\n[+] IEEE 754 单精度(32位):\n");
        printEncodingSteps(0, b32, (double)f);
        printFloatFields(0, b32, 1, (double)f);
        char binB[40];
        toBinaryString(b32, 32, binB);
        if (wantHex)
            printf("\n[+] 结果: 0x%08X (十六进制) = %.9g (十进制)\n",
                   (unsigned)b32, (double)f);
        else {
            printf("\n[+] 结果: ");
            printGrouped(binB);
            printf("    十六进制: 0x%08X\n", (unsigned)b32);
        }
    }
}

/* ---- 功能 14/17: 二进制/十六进制 -> 十进制 (共用流水线, 自动识别) ---- */
static void floatToDec(void)
{
    int isDouble = choosePrecision();
    if (isDouble < 0) return;
    uint64_t bits;
    if (!readFloatBits(isDouble, &bits)) return;
    printf("\n[+] 解码为十进制:\n");
    printDecodingSteps(isDouble, bits);
    if (isDouble) {
        double d;
        memcpy(&d, &bits, sizeof d);
        printFloatFields(1, bits, 0, d);
        printf("\n[+] 十进制浮点结果: %.17g\n", d);
    } else {
        uint32_t b32 = (uint32_t)bits;
        float f;
        memcpy(&f, &b32, sizeof f);
        printFloatFields(0, b32, 0, (double)f);
        printf("\n[+] 十进制浮点结果: %g\n", (double)f);
    }
}

/* ---- 功能 15/16: 二进制 <-> 十六进制 (4位分组演示) ---- */
static void floatBinHexConv(int binToHex)
{
    int isDouble = choosePrecision();
    if (isDouble < 0) return;
    uint64_t bits;
    if (!readFloatBits(isDouble, &bits)) return;
    int total = isDouble ? 64 : 32;
    char full[65];
    toBinaryString(bits, total, full);
    if (binToHex) {
        printf("    推导 (4位一组): \n");
        char hex[20];
        binGroupsToBase(full, 4, hex);
        printf(isDouble ? "\n[+] IEEE 754 十六进制: 0x%s\n" : "\n[+] IEEE 754 十六进制: 0x%s\n", hex);
    } else {
        char hex[20];
        if (isDouble) snprintf(hex, sizeof hex, "%016llX", (unsigned long long)bits);
        else          snprintf(hex, sizeof hex, "%08X", (unsigned)(uint32_t)bits);
        printf("    推导:\n");
        char exp[300];
        baseDigitsToBin(hex, 4, 16, exp);
        char padded[80] = {0};
        int el = (int)strlen(exp);
        if (el < total) { memset(padded, '0', total - el); strcpy(padded + (total - el), exp); }
        else strcpy(padded, exp);
        printf("\n[+] IEEE 754 二进制: ");
        printGrouped(padded);
    }
}

/* ======================== 主函数 ======================== */

int main(void)
{
    int choice = -1;
    while (choice != 0) {
        printf("\n====== 进制转换工具 ======\n");
        printf(" 1. 整数: 二进制 -> 十进制\n");
        printf(" 2. 整数: 十进制 -> 二进制\n");
        printf("\n");
        printf(" 3. 整数: 八进制 -> 十进制\n");
        printf(" 4. 整数: 十进制 -> 八进制\n");
        printf(" 5. 整数: 二进制 -> 八进制\n");
        printf(" 6. 整数: 八进制 -> 二进制\n");
        printf("\n");
        printf(" 7. 整数: 16进制 -> 十进制\n");
        printf(" 8. 整数: 十进制 -> 16进制\n");
        printf(" 9. 整数: 二进制 -> 16进制\n");
        printf("10. 整数: 16进制 -> 二进制\n");
        printf("11. 整数: 八进制 -> 16进制\n");
        printf("12. 整数: 16进制 -> 八进制\n");
        printf("\n");
        printf("13. 浮点: 十进制 -> 二进制 (IEEE 754)\n");
        printf("14. 浮点: 二进制 -> 十进制 (IEEE 754)\n");
        printf("\n");
        printf("15. 浮点: 二进制 -> 16进制 (IEEE 754)\n");
        printf("16. 浮点: 16进制 -> 二进制 (IEEE 754)\n");
        printf("17. 浮点: 16进制 -> 十进制 (IEEE 754)\n");
        printf("18. 浮点: 十进制 -> 16进制 (IEEE 754)\n");
        printf(" 0. 退出\n");
        printf("======================================\n");
        printf("请选择功能 (0-18): ");
        char input[32];
        if (!readLine(input, sizeof(input))) break;
        choice = atoi(input);
        switch (choice) {
            case 1: intBinToDec(); break;
            case 2: intDecToBin(); break;
            case 3: intOctToDec(); break;
            case 4: intDecToOct(); break;
            case 5: intBinToOct(); break;
            case 6: intOctToBin(); break;
            case 7: intHexToDec(); break;
            case 8: intDecToHex(); break;
            case 9: intBinToHex(); break;
            case 10: intHexToBin(); break;
            case 11: intOctToHex(); break;
            case 12: intHexToOct(); break;
            case 13: floatDecToConv(0); break;
            case 14: floatToDec(); break;
            case 15: floatBinHexConv(1); break;
            case 16: floatBinHexConv(0); break;
            case 17: floatToDec(); break;
            case 18: floatDecToConv(1); break;
            case 0: printf("退出程序。\n"); break;
            default: printf("[!] 无效的选择，请重新输入。\n");
        }
    }
    return 0;
}
