/*
 * 十进制 ↔ 十六进制 转换工具
 *
 * 功能:
 *   1. 十进制 → 十六进制 (整型, 支持 8/16/32/64 位, 补码编码, 支持负数)
 *   2. 十六进制 → 十进制 (整型, 同上)
 *   3. 十进制 → 十六进制 (浮点, 支持单精度f/双精度d, IEEE 754 位模式)
 *   4. 十六进制 → 十进制 (浮点, 同上)
 *
 * 编译: gcc converter.c -o converter -lm
 *       (Linux 需要 -lm; Windows MinGW/MSVC 不需要)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <errno.h>

/* ===== 全局状态 ===== */
static int int_width   = 8;    /* 整型当前位宽, 默认 8 位 */
static char float_prec = 'f';  /* 浮点精度: 'f'=单精度(32位) / 'd'=双精度(64位) */
static int float_width = 32;   /* 浮点位宽: 32 或 64, 与 float_prec 联动 */

/* ================================================================
 *  工具函数
 * ================================================================ */

/*
 * 跨平台大小写不敏感前缀比较
 * MSVC 没有 strncasecmp, 用 _strnicmp; 其他编译器用 POSIX 的 strncasecmp
 */
#if defined(_MSC_VER)
  #define strncasecmp _strnicmp
#endif

/*
 * 读取一行输入到 buf, 自动去除末尾的 \r\n
 * 返回 1 表示成功, 0 表示 EOF (Ctrl+D / Ctrl+Z)
 */
static int readLine(char *buf, size_t size)
{
    if (!fgets(buf, (int)size, stdin))
        return 0;
    buf[strcspn(buf, "\r\n")] = '\0';
    return 1;
}

/*
 * 删除字符串中的所有空白字符(空格、Tab 等), 原地修改
 * 返回指向同一缓冲区的指针(内容已去除空白)
 */
static char *stripSpaces(char *s)
{
    char *out = s;
    size_t n = 0;
    while (*s) {
        if (!isspace((unsigned char)*s))
            out[n++] = *s;
        s++;
    }
    out[n] = '\0';
    return out;
}

/*
 * 严格读取一个整数, 拒绝尾随垃圾字符(如 "12abc")
 *
 * 返回值 (三态):
 *   1  → 成功, 结果存入 *out
 *   0  → 用户输入了内容但格式非法 (如 "abc", "12x")
 *  -1  → EOF (Ctrl+D / Ctrl+Z)
 *
 * 调用方应根据返回值区分处理:
 *   -1 → 静默返回, 不要弹"无效输入"
 *    0 → 提示格式错误
 *    1 → 正常使用 *out
 */
static int readIntStrict(const char *prompt, long long *out)
{
    char line[64];
    char *end;
    long long val;

    fputs(prompt, stdout);
    if (!readLine(line, sizeof line))
        return -1;                              /* EOF: 返回 -1, 与非法输入区分 */

    stripSpaces(line);
    if (line[0] == '\0')
        return 0;                               /* 空输入视为非法 */

    errno = 0;
    val = strtoll(line, &end, 10);
    if (end == line || *end != '\0' || errno == ERANGE)
        return 0;                               /* 格式错误或溢出 */

    *out = val;
    return 1;
}

/*
 * 根据位宽 w 返回对应的无符号掩码
 * 例: w=8 → 0xFF, w=16 → 0xFFFF, w=64 → 0xFFFFFFFFFFFFFFFF
 * 注意: 64 位时不能做 (1ULL << 64), 那是未定义行为, 必须特判
 */
static unsigned long long widthMask(int w)
{
    return (w >= 64) ? ~0ULL : ((1ULL << w) - 1);
}

/*
 * 位宽 w 对应的有符号最大值
 * 例: w=8 → 127, w=64 → LLONG_MAX
 */
static long long maxSigned(int w)
{
    return (w == 64) ? LLONG_MAX : ((1LL << (w - 1)) - 1);
}

/*
 * 位宽 w 对应的有符号最小值
 * 例: w=8 → -128, w=64 → LLONG_MIN
 */
static long long minSigned(int w)
{
    return (w == 64) ? LLONG_MIN : (-(1LL << (w - 1)));
}

/* ================================================================
 *  位宽 / 精度选择
 * ================================================================ */

/*
 * 整型位宽选择: 只接受 8 / 16 / 32 / 64
 * 输入其他值时保持当前位宽不变
 * EOF 时静默返回, 不弹误导性的"无效位宽"提示
 */
static void chooseIntWidth(void)
{
    long long w;
    int rc;

    rc = readIntStrict("请选择整型位宽 (8/16/32/64), 当前为 %d 位: ", &w);

    /*
     * 【修复瑕疵 #1】区分 EOF 和非法输入:
     * rc == -1 → EOF, 静默返回, 不输出任何提示
     * rc ==  0 → 用户输入了但格式不对, 提示"无效位宽"
     * rc ==  1 → 成功, 校验值是否合法
     */
    if (rc == -1)
        return;                                 /* EOF: 静默退出, 不误导用户 */

    if (rc == 1 && (w == 8 || w == 16 || w == 32 || w == 64)) {
        int_width = (int)w;
    } else {
        printf("无效位宽, 保持当前 %d 位。\n", int_width);
    }
}

/*
 * 浮点精度选择: 输入 f/F → 单精度(32位), d/D → 双精度(64位)
 * 位宽由精度自动决定, 无需用户手动选择
 */
static void chooseFloatPrecision(void)
{
    char line[64];
    char c;

    printf("请选择浮点精度 (f=单精度32位 / d=双精度64位), 当前为 %c: ",
           float_prec);
    if (!readLine(line, sizeof line)) return;

    if (sscanf(line, " %c", &c) == 1 &&
        (c == 'f' || c == 'F' || c == 'd' || c == 'D')) {
        float_prec = (char)tolower((unsigned char)c);
        float_width = (float_prec == 'f') ? 32 : 64;
    } else {
        printf("无效精度, 保持当前 %c (%d位)。\n", float_prec, float_width);
    }
}

/* ================================================================
 *  选项 1: 十进制 → 十六进制 (整型)
 *
 *  原理:
 *    将用户输入的十进制有符号整数, 按补码编码转为十六进制显示
 *    补码编码: 负数 → 2^w + value (w 为位宽)
 *    等价于: bits = (unsigned long long)value & widthMask(w)
 *
 *  示例 (8位):
 *    输入 -5  → 补码 0xFB → 输出 FB
 *    输入 127 → 补码 0x7F → 输出 7F
 *    输入 -128 → 补码 0x80 → 输出 80
 * ================================================================ */
static void decToHexInt(void)
{
    char line[128];
    char *s;
    char *end;
    long long value;
    unsigned long long bits;
    int hexDigits;

    printf("\n===== 十进制 → 十六进制 (整型, %d位) =====\n", int_width);
    printf("有符号范围: [%lld, %lld]\n",
           minSigned(int_width), maxSigned(int_width));
    printf("请输入十进制数 (支持负数): ");
    if (!readLine(line, sizeof line)) return;

    s = stripSpaces(line);

    /* 用 strtoll 解析十进制整数, end 指向第一个非数字字符 */
    errno = 0;
    value = strtoll(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE) {
        printf("错误: \"%s\" 不是有效的十进制整数！\n", s);
        return;
    }

    /* 超出当前位宽范围时给出警告, 但仍按补码截断处理 */
    if (value > maxSigned(int_width) || value < minSigned(int_width)) {
        printf("警告: %lld 超出 %d 位有符号范围, 将按补码截断。\n",
               value, int_width);
    }

    /*
     * 补码编码核心:
     * C 语言中, 将有符号负数赋值给无符号类型, 标准保证结果为 2^N + value
     * 再按位与掩码截取低 w 位, 即得到该位宽下的补码表示
     * 例: 8位下 value=-5
     *   (unsigned long long)(-5) = 0xFFFFFFFFFFFFFFFB
     *   & 0xFF = 0xFB
     */
    bits = (unsigned long long)value & widthMask(int_width);

    /* 每 4 个二进制位对应 1 个十六进制位 */
    hexDigits = int_width / 4;

    printf("十进制: %lld\n", value);
    printf("十六进制: %0*llX\n", hexDigits, bits);
}

/* ================================================================
 *  选项 2: 十六进制 → 十进制 (整型)
 *
 *  原理:
 *    将用户输入的十六进制数解析为无符号值, 再按补码规则解释为有符号数
 *    如果无符号值 > 有符号最大值(即最高位为1), 则实际值 = 无符号值 - 2^w
 *
 *  支持:
 *    - 可选的 0x/0X 前缀
 *    - 可选的负号前缀 (如 -FF, 直接取负)
 *    - 溢出检查 (输入超过 64 位时报错)
 *
 *  示例 (8位):
 *    输入 FF  → 无符号 255, 255 > 127 → 255-256 = -1
 *    输入 80  → 无符号 128, 128 > 127 → 128-256 = -128
 *    输入 7F  → 无符号 127, 127 ≤ 127 → 127
 * ================================================================ */
static void hexToDecInt(void)
{
    char line[128];
    char *s;
    char *p;
    int is_negative = 0;
    unsigned long long uval;
    long long result;

    printf("\n===== 十六进制 → 十进制 (整型, %d位) =====\n", int_width);
    printf("请输入十六进制数 (可带0x前缀, 支持负号): ");
    if (!readLine(line, sizeof line)) return;

    s = stripSpaces(line);
    p = s;

    /* 处理可选的负号/正号前缀 */
    if (*p == '-')      { is_negative = 1; p++; }
    else if (*p == '+') { p++; }

    /* 去掉可选的 0x / 0X 前缀 */
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
        p += 2;

    if (*p == '\0') {
        printf("错误: 输入为空！\n");
        return;
    }

    /* 逐字符校验: 每个字符必须是合法的十六进制数字 (0-9, A-F, a-f) */
    for (char *q = p; *q; q++) {
        if (!isxdigit((unsigned char)*q)) {
            printf("错误: '%c' 不是有效的十六进制字符！\n", *q);
            return;
        }
    }

    /*
     * 解析十六进制字符串为无符号整数, 并检查是否超出 unsigned long long 范围
     * errno == ERANGE 表示输入超长, strtoull 饱和返回 ULLONG_MAX
     */
    errno = 0;
    uval = strtoull(p, NULL, 16);
    if (errno == ERANGE) {
        printf("错误: 输入数值超出 64 位无符号整数范围！\n");
        return;
    }

    if (is_negative) {
        /*
         * 显式负号: 先截断到位宽, 再取负
         * 例: 输入 -5, 位宽 8
         *   uval = 5, 截断后仍为 5, 取负得 -5
         */
        result = -(long long)(uval & widthMask(int_width));
    } else {
        /*
         * 无显式负号: 按补码规则解释
         * 先截断到位宽, 再判断最高位是否为 1
         * 如果无符号值 > 有符号最大值 → 最高位为 1 → 解释为负数
         * 实际值 = 无符号值 - 2^位宽
         */
        uval &= widthMask(int_width);
        if (int_width == 64) {
            /* 64 位特殊处理: 直接强制转换即可 (C 标准保证) */
            result = (long long)uval;
        } else if (uval > (unsigned long long)maxSigned(int_width)) {
            result = (long long)uval - (1LL << int_width);
        } else {
            result = (long long)uval;
        }
    }

    printf("十六进制: %s\n", s);
    printf("十进制: %lld\n", result);
}

/* ================================================================
 *  选项 3: 十进制 → 十六进制 (浮点, IEEE 754 位模式)
 *
 *  原理:
 *    将用户输入的十进制浮点数, 通过 memcpy 取其 IEEE 754 内存位模式,
 *    然后以十六进制形式输出
 *    - 单精度 (f): float → 32 位 → 8 个十六进制位
 *    - 双精度 (d): double → 64 位 → 16 个十六进制位
 *
 *  f/F 后缀处理:
 *    用户输入如 "3.14f" 或 "3.14F" 时, 末尾的 f/F 是浮点类型后缀
 *    需要去掉后再用 strtod 解析
 *    但必须排除 "inf" / "infinity" 这种特殊值, 它们的末尾 f 是数值的一部分!
 *
 *  示例:
 *    单精度: 12.5  → 0x41480000
 *    双精度: 3.14  → 0x40091EB851EB851F
 * ================================================================ */
static void decToHexFloat(void)
{
    char line[128];
    char *s;
    char *end;
    size_t len;
    double v;

    printf("\n===== 十进制 → 十六进制 (%s精度, %d位) =====\n",
           float_prec == 'f' ? "单" : "双", float_width);
    printf("请输入十进制浮点数 (可带后缀 f/F, 如 3.14f): ");
    if (!readLine(line, sizeof line)) return;

    s = stripSpaces(line);
    len = strlen(s);

    /*
     * 处理 f/F 后缀:
     * 如果最后一个字符是 f 或 F, 需要判断它到底是:
     *   (a) 浮点类型后缀 (如 "3.14f") → 应该去掉
     *   (b) "inf" / "infinity" 的一部分 → 不能去掉!
     *
     * 判断方法: 用 strncasecmp 检查字符串是否以 "inf" 开头
     * 如果以 "inf" 开头, 说明末尾的 f 属于 "inf", 不剥离
     */
    if (len > 0 && (s[len - 1] == 'f' || s[len - 1] == 'F')
        && strncasecmp(s, "inf", 3) != 0) {
        s[--len] = '\0';
    }

    /* 用 strtod 解析浮点数 */
    errno = 0;
    v = strtod(s, &end);
    if (end == s || *end != '\0') {
        printf("错误: \"%s\" 不是有效的浮点数！\n", s);
        return;
    }

    if (float_prec == 'f') {
        /*
         * 单精度转换:
         * 1. 将 double 截断为 float (可能损失精度)
         * 2. 用 memcpy 将 float 的 4 字节位模式复制到 unsigned int
         * 3. 以十六进制输出
         *
         * 为什么用 memcpy 而不是指针强转?
         * 因为 C 标准中通过指针强转访问不同类型是"严格别名"违规(UB)
         * memcpy 是标准保证安全的做法, 编译器也会优化掉实际拷贝
         */
        float fv = (float)v;
        unsigned int bits;
        memcpy(&bits, &fv, sizeof bits);

        printf("十进制: ");
        if (isfinite(fv))
            printf("%.9gf\n", fv);    /* 有限值: 保留足够精度 + f 后缀 */
        else
            printf("%g\n", fv);       /* inf / nan: 直接输出 */
        printf("十六进制: %08X\n", bits);
    } else {
        /*
         * 双精度转换:
         * 用 memcpy 将 double 的 8 字节位模式复制到 unsigned long long
         */
        unsigned long long bits;
        memcpy(&bits, &v, sizeof bits);

        /* 有限值和特殊值分开输出, 避免重复打印 */
        printf("十进制: ");
        if (isfinite(v))
            printf("%.17g\n", v);     /* 有限值: 17 位有效数字(双精度极限) */
        else
            printf("%g\n", v);        /* inf / nan: 直接输出 */
        printf("十六进制: %016llX\n", bits);
    }
}

/* ================================================================
 *  选项 4: 十六进制 → 十进制 (浮点, IEEE 754 位模式)
 *
 *  原理:
 *    将用户输入的十六进制位模式, 通过 memcpy 重新解释为 float/double
 *    - 单精度: 需要 8 个十六进制位 (32 位)
 *    - 双精度: 需要 16 个十六进制位 (64 位)
 *
 *  重要区别:
 *    这里不能去掉末尾的 f/F! 因为 F 本身就是合法的十六进制数字
 *    例: 输入 "4048F5C3" 末尾的 3 是数字, 但如果输入 "4120000F"
 *    末尾的 F 是十六进制的 15, 绝不是浮点后缀!
 *
 *  示例:
 *    单精度: 41480000     → 12.5
 *    双精度: 40091EB851EB851F → 3.14
 * ================================================================ */
static void hexToDecFloat(void)
{
    char line[128];
    char *s, *p;
    unsigned long long uval;
    int expectDigits = (float_prec == 'f') ? 8 : 16;

    printf("\n===== 十六进制 → 十进制 (%s精度, %d位) =====\n",
           float_prec == 'f' ? "单" : "双", float_width);
    printf("请输入十六进制位模式 (需要 %d 个十六进制位, 可带0x前缀): ",
           expectDigits);
    if (!readLine(line, sizeof line)) return;

    s = stripSpaces(line);
    p = s;

    /* 去掉可选的 0x / 0X 前缀 */
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
        p += 2;

    if (*p == '\0') {
        printf("错误: 输入为空！\n");
        return;
    }

    /*
     * 逐字符校验十六进制数字
     * 注意: 这里绝对不能去掉末尾的 f/F, 因为 F 是合法的十六进制数字!
     * 这是与 decToHexFloat 的关键区别
     */
    for (char *q = p; *q; q++) {
        if (!isxdigit((unsigned char)*q)) {
            printf("错误: '%c' 不是有效的十六进制字符！\n", *q);
            return;
        }
    }

    /* 位数不匹配时给出警告(但不阻止解析) */
    if ((int)strlen(p) != expectDigits) {
        printf("警告: 输入了 %d 位, %s精度应为 %d 位, 将按数值解析。\n",
               (int)strlen(p),
               float_prec == 'f' ? "单" : "双",
               expectDigits);
    }

    /*
     * 解析十六进制并检查溢出:
     * strtoull 在输入超出 unsigned long long 范围时返回 ULLONG_MAX 并设置 errno
     * 必须检查 errno, 否则超长输入会被静默截断
     */
    errno = 0;
    uval = strtoull(p, NULL, 16);
    if (errno == ERANGE) {
        printf("错误: 输入数值超出 64 位无符号整数范围！\n");
        return;
    }

    if (float_prec == 'f') {
        /*
         * 单精度: 检查是否超出 32 位范围
         * 然后将 32 位无符号整数通过 memcpy 重新解释为 float
         */
        if (uval > 0xFFFFFFFFULL) {
            printf("错误: 超出单精度 32 位范围！\n");
            return;
        }
        unsigned int bits = (unsigned int)uval;
        float fv;
        memcpy(&fv, &bits, sizeof fv);

        printf("十六进制: %s\n", s);
        if (isfinite(fv))
            printf("十进制: %.9gf\n", fv);
        else
            printf("十进制: %g\n", fv);
    } else {
        /*
         * 双精度: 将 64 位无符号整数通过 memcpy 重新解释为 double
         */
        unsigned long long bits = uval;
        double dv;
        memcpy(&dv, &bits, sizeof dv);

        printf("十六进制: %s\n", s);
        if (isfinite(dv))
            printf("十进制: %.17g\n", dv);
        else
            printf("十进制: %g\n", dv);
    }
}

/* ================================================================
 *  主函数: 菜单循环
 * ================================================================ */
int main(void)
{
    long long choice;
    int rc;

    printf("====================================\n");
    printf("   十进制 ↔ 十六进制 转换工具\n");
    printf("   (整型补码 + IEEE 754 浮点)\n");
    printf("====================================\n");

    while (1) {
        printf("\n---------- 主菜单 ----------\n");
        printf("  1. 十进制 → 十六进制 (整型)\n");
        printf("  2. 十六进制 → 十进制 (整型)\n");
        printf("  3. 十进制 → 十六进制 (浮点)\n");
        printf("  4. 十六进制 → 十进制 (浮点)\n");
        printf("  0. 退出\n");

        /*
         * 【修复瑕疵 #2】主菜单复用 readIntStrict, 消除重复代码:
         * 旧代码: 内联写了一遍 strtoll + *end=='\0' + errno 检查 (十几行)
         * 新代码: 直接调用 readIntStrict, 根据三态返回值分别处理
         *
         * rc == -1 → EOF (Ctrl+D/Ctrl+Z), 跳出循环优雅退出
         * rc ==  0 → 用户输入了但格式非法, 提示重新输入
         * rc ==  1 → 成功, choice 已填入
         */
        rc = readIntStrict("请选择 (0-4): ", &choice);
        if (rc == -1) {
            printf("\n");                       /* EOF 时换行, 避免提示符黏连 */
            break;
        }
        if (rc == 0) {
            printf("输入无效, 请重新输入。\n");
            continue;
        }

        switch (choice) {
            case 1:
                chooseIntWidth();       /* 先选位宽 */
                decToHexInt();          /* 再执行转换 */
                break;
            case 2:
                chooseIntWidth();
                hexToDecInt();
                break;
            case 3:
                chooseFloatPrecision(); /* 先选精度(f/d), 位宽自动确定 */
                decToHexFloat();
                break;
            case 4:
                chooseFloatPrecision();
                hexToDecFloat();
                break;
            case 0:
                printf("程序已退出。\n");
                return 0;
            default:
                printf("无效选项, 请重新输入。\n");
        }
    }

    return 0;
}