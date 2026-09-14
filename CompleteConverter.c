#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <ctype.h>

/* ======================== 通用工具函数 ======================== */

static void myStrRev(char *str) {
    if (!str) return;
    int len = strlen(str);
    for (int i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = temp;
    }
}

static int readLine(char *buffer, size_t size) {
    if (fgets(buffer, (int)size, stdin) == NULL) return 0;
    size_t len = strlen(buffer);
    while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
        buffer[--len] = '\0';
    }
    return 1;
}

static void stripSpaces(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (!isspace((unsigned char)*src)) *dst++ = *src;
        src++;
    }
    *dst = '\0';
}

static int chooseWidth(void) {
    char line[128];
    int width = 0;
    while (1) {
        printf("请选择位宽 (8/16/32/64): ");
        if (!readLine(line, sizeof(line))) return -1;
        stripSpaces(line);
        width = atoi(line);
        if (width == 8 || width == 16 || width == 32 || width == 64) return width;
        printf("[!] 错误: 请输入 8, 16, 32 或 64。\n");
    }
}

/* ★ 修复核心：独立的解读方式读取函数，使用独立缓冲区，不再污染数字串 */
static int readMode(void) {
    char mline[32];
    int mode = 0;
    while (mode < 1 || mode > 3) {
        printf("解读方式 (1=正数 / 2=负数(绝对值) / 3=补码解读): ");
        if (!readLine(mline, sizeof(mline))) return -1; // EOF
        stripSpaces(mline);
        mode = atoi(mline);
        if (mode < 1 || mode > 3) printf("[!] 请输入 1、2 或 3。\n");
    }
    return mode;
}

/* ======================== 整数转换基础算法 ======================== */

static long long binToLongLong(const char *bin, int width) {
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

static void longLongToBin(long long val, char *bin, size_t size, int width) {
    (void)size;
    if (width < 64) val &= ((1ULL << width) - 1);
    bin[width] = '\0';
    unsigned long long uVal = (unsigned long long)val;
    for (int i = width - 1; i >= 0; i--) {
        bin[i] = (uVal & 1) ? '1' : '0';
        uVal >>= 1;
    }
}

static void longLongToHex(long long val, char *hex, size_t size, int width) {
    if (width < 64) val &= ((1ULL << width) - 1);
    if (width <= 8) snprintf(hex, size, "%02X", (unsigned)(val & 0xFF));
    else if (width <= 16) snprintf(hex, size, "%04X", (unsigned)(val & 0xFFFF));
    else if (width <= 32) snprintf(hex, size, "%08X", (unsigned)(val & 0xFFFFFFFF));
    else snprintf(hex, size, "%016llX", (unsigned long long)val);
}

static long long hexToLongLong(const char *hex, int width) {
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

static void octToBinStr(const char *oct, char *bin) {
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
static void binStrToOctStr(const char *bin, char *oct, int width) {
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

static int isHexValid(const char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (!isxdigit((unsigned char)str[i])) return 0;
    }
    return 1;
}

/* ======================== 1. 二进制 <-> 十进制 ======================== */
static void intBinToDec(void) {
    int width = chooseWidth(); if (width < 0) return;
    char numStr[256];                          // ★ 独立缓冲区存放二进制串
    printf("请输入二进制串 (可带空格): ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    for (int i = 0; numStr[i]; i++)
        if (numStr[i] != '0' && numStr[i] != '1') { printf("[!] 非法二进制。\n"); return; }

    int mode = readMode();                     // ★ 不再覆盖 numStr
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
    printf("\n[+] 十进制结果: %lld\n", dec);
}

static void intDecToBin(void) {
    int width = chooseWidth(); if (width < 0) return;
    char line[128]; long long dec;
    printf("请输入十进制整数: ");
    if (!readLine(line, sizeof(line))) return;
    stripSpaces(line);
    dec = atoll(line);

    char bin[136];
    longLongToBin(dec, bin, sizeof(bin), width);
    printf("\n[+] 二进制结果: %s\n", bin);
}

/* ======================== 2. 八进制 <-> 十/二进制 ======================== */
static void intOctToDec(void) {
    int width = chooseWidth(); if (width < 0) return;
    char numStr[256], bin[1024];               // ★ 独立缓冲区
    printf("请输入八进制串: ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    for (int i = 0; numStr[i]; i++)
        if (numStr[i] < '0' || numStr[i] > '7') { printf("[!] 非法八进制。\n"); return; }

    int mode = readMode();                     // ★ 不再覆盖 numStr
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
}

static void intDecToOct(void) {
    int width = chooseWidth(); if (width < 0) return;
    char line[128]; long long dec;
    printf("请输入十进制整数: ");
    if (!readLine(line, sizeof(line))) return;
    stripSpaces(line);
    dec = atoll(line);

    char bin[136], oct[256];
    longLongToBin(dec, bin, sizeof(bin), width);
    binStrToOctStr(bin, oct, width);
    printf("\n[+] 八进制结果: %s\n", oct);
}

/* ======================== 3. 二进制 <-> 八进制 ======================== */
static void intBinToOct(void) {
    int width = chooseWidth(); if (width < 0) return;
    char numStr[256];                          // ★ 独立缓冲区
    printf("请输入二进制串 (可带空格): ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    for (int i = 0; numStr[i]; i++)
        if (numStr[i] != '0' && numStr[i] != '1') { printf("[!] 非法二进制。\n"); return; }

    int mode = readMode();                     // ★ 不再覆盖 numStr
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
}

static void intOctToBin(void) {
    int width = chooseWidth(); if (width < 0) return;
    char numStr[256], bin[1024];               // ★ 独立缓冲区
    printf("请输入八进制串: ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    for (int i = 0; numStr[i]; i++)
        if (numStr[i] < '0' || numStr[i] > '7') { printf("[!] 非法八进制。\n"); return; }

    int mode = readMode();                     // ★ 不再覆盖 numStr
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
}

/* ======================== 4. 十六进制相关 ======================== */
static void intHexToDec(void) {
    int width = chooseWidth(); if (width < 0) return;
    char numStr[256];                          // ★ 独立缓冲区
    printf("请输入十六进制串: ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    if (!isHexValid(numStr)) { printf("[!] 非法十六进制。\n"); return; }

    int mode = readMode();                     // ★ 不再覆盖 numStr
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
}

static void intDecToHex(void) {
    int width = chooseWidth(); if (width < 0) return;
    char line[128]; long long dec;
    printf("请输入十进制整数: ");
    if (!readLine(line, sizeof(line))) return;
    stripSpaces(line);
    dec = atoll(line);

    char hex[136];
    longLongToHex(dec, hex, sizeof(hex), width);
    printf("\n[+] 十六进制结果: %s\n", hex);
}

static void intBinToHex(void) {
    int width = chooseWidth(); if (width < 0) return;
    char numStr[256];                          // ★ 独立缓冲区
    printf("请输入二进制串 (可带空格): ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    for (int i = 0; numStr[i]; i++)
        if (numStr[i] != '0' && numStr[i] != '1') { printf("[!] 非法二进制。\n"); return; }

    int mode = readMode();                     // ★ 不再覆盖 numStr
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
}

static void intHexToBin(void) {
    int width = chooseWidth(); if (width < 0) return;
    char numStr[256];                          // ★ 独立缓冲区
    printf("请输入十六进制串: ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    if (!isHexValid(numStr)) { printf("[!] 非法十六进制。\n"); return; }

    int mode = readMode();                     // ★ 不再覆盖 numStr
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
}

static void intOctToHex(void) {
    int width = chooseWidth(); if (width < 0) return;
    char numStr[256], bin[1024];               // ★ 独立缓冲区
    printf("请输入八进制串: ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    for (int i = 0; numStr[i]; i++)
        if (numStr[i] < '0' || numStr[i] > '7') { printf("[!] 非法八进制。\n"); return; }

    int mode = readMode();                     // ★ 不再覆盖 numStr
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
}

static void intHexToOct(void) {
    int width = chooseWidth(); if (width < 0) return;
    char numStr[256];                          // ★ 独立缓冲区
    printf("请输入十六进制串: ");
    if (!readLine(numStr, sizeof(numStr))) return;
    stripSpaces(numStr);
    if (!isHexValid(numStr)) { printf("[!] 非法十六进制。\n"); return; }

    int mode = readMode();                     // ★ 不再覆盖 numStr
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
}

/* ======================== 5. IEEE 754 浮点转换 ======================== */
static void printFloatFields(uint32_t bits) {
    int sign = (bits >> 31) & 1;
    int exp = (bits >> 23) & 0xFF;
    uint32_t mant = bits & 0x7FFFFF;
    printf("     -> 符号: %d, 指数: %d (实际: %d), 尾数: 0x%06X\n",
           sign, exp, (exp == 0 ? 0 : exp - 127), mant);
}

static void floatDecToBin(void) {
    double val; char line[128];
    printf("请输入十进制浮点数: ");
    if (!readLine(line, sizeof(line))) return;
    stripSpaces(line);
    char *end;
    val = strtod(line, &end);
    if (*end != '\0') { printf("[!] 无效浮点数。\n"); return; }

    float f = (float)val;
    uint32_t bits;
    memcpy(&bits, &f, sizeof(uint32_t));

    char bin[40] = {0};
    for (int i = 31; i >= 0; i--) {
        bin[31 - i] = ((bits >> i) & 1) ? '1' : '0';
    }
    char formatted[40] = {0};
    strncpy(formatted, bin, 1);
    strcat(formatted, " ");
    strncat(formatted, bin + 1, 8);
    strcat(formatted, " ");
    strcat(formatted, bin + 9);

    printf("\n[+] IEEE 754 二进制: %s\n", formatted);
    printFloatFields(bits);
}

static void floatBinToDec(void) {
    char line[128];
    printf("请输入32位二进制串: ");
    if (!readLine(line, sizeof(line))) return;
    stripSpaces(line);
    if (strlen(line) != 32) { printf("[!] 必须是32位。\n"); return; }

    uint32_t bits = 0;
    for (int i = 0; i < 32; i++) bits = (bits << 1) | (line[i] - '0');

    float f;
    memcpy(&f, &bits, sizeof(uint32_t));
    printf("\n[+] 十进制浮点结果: %g\n", f);
    printFloatFields(bits);
}

static void floatBinToHex(void) {
    char line[128];
    printf("请输入32位二进制串: ");
    if (!readLine(line, sizeof(line))) return;
    stripSpaces(line);
    if (strlen(line) != 32) { printf("[!] 必须是32位。\n"); return; }

    uint32_t bits = 0;
    for (int i = 0; i < 32; i++) bits = (bits << 1) | (line[i] - '0');
    printf("\n[+] IEEE 754 十六进制: %08X\n", bits);
    printFloatFields(bits);
}

static void floatHexToBin(void) {
    char line[128];
    printf("请输入8位十六进制串 (如 C0A00000): ");
    if (!readLine(line, sizeof(line))) return;
    stripSpaces(line);
    if (!isHexValid(line) || strlen(line) != 8) { printf("[!] 必须是8位十六进制。\n"); return; }

    uint32_t bits = strtoul(line, NULL, 16);
    char bin[40] = {0};
    for (int i = 31; i >= 0; i--) bin[31 - i] = ((bits >> i) & 1) ? '1' : '0';
    char formatted[40] = {0};
    strncpy(formatted, bin, 1);
    strcat(formatted, " ");
    strncat(formatted, bin + 1, 8);
    strcat(formatted, " ");
    strcat(formatted, bin + 9);

    printf("\n[+] IEEE 754 二进制: %s\n", formatted);
    printFloatFields(bits);
}

static void floatHexToDec(void) {
    char line[128];
    printf("请输入8位十六进制串: ");
    if (!readLine(line, sizeof(line))) return;
    stripSpaces(line);
    if (!isHexValid(line) || strlen(line) != 8) { printf("[!] 必须是8位十六进制。\n"); return; }

    uint32_t bits = strtoul(line, NULL, 16);
    float f;
    memcpy(&f, &bits, sizeof(uint32_t));
    printf("\n[+] 十进制浮点结果: %g\n", f);
    printFloatFields(bits);
}

static void floatDecToHex(void) {
    double val; char line[128];
    printf("请输入十进制浮点数: ");
    if (!readLine(line, sizeof(line))) return;
    stripSpaces(line);
    char *end;
    val = strtod(line, &end);
    if (*end != '\0') { printf("[!] 无效浮点数。\n"); return; }

    float f = (float)val;
    uint32_t bits;
    memcpy(&bits, &f, sizeof(uint32_t));
    printf("\n[+] IEEE 754 十六进制: %08X\n", bits);
    printFloatFields(bits);
}

/* ======================== 主函数 ======================== */
int main(void) {
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
            case 13: floatDecToBin(); break;
            case 14: floatBinToDec(); break;
            case 15: floatBinToHex(); break;
            case 16: floatHexToBin(); break;
            case 17: floatHexToDec(); break;
            case 18: floatDecToHex(); break;
            case 0: printf("退出程序。\n"); break;
            default: printf("[!] 无效的选择，请重新输入。\n");
        }
    }
    return 0;
}
