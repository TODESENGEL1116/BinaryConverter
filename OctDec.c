#include<stdio.h>
#include<string.h>
#include<math.h>
#include<stdlib.h>

/*全局变量 当前位宽*/
int bit_width=8;

/* 计算当前位宽下有符号数的范围 */
static long long get_max_signed(void) {
    return (1LL << (bit_width - 1)) - 1;
}

static long long get_min_signed(void) {
    return -(1LL << (bit_width - 1));
}

/* 计算当前位宽下无符号数的最大值 */
static long long get_max_unsigned(void) {
    return (1LL << bit_width) - 1;
}

/* ==================== 选项 1：选择位宽 ==================== */
void select_bit_width(void) {
    int w;
    printf("\n===== 选择位宽 =====\n");
    printf("当前位宽: %d 位\n", bit_width);
    printf("有符号范围: [%lld, %lld]\n", get_min_signed(), get_max_signed());
    printf("请输入新的位宽 (1~63): ");
    if (scanf("%d", &w) != 1) {
        while (getchar() != '\n')                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       
        printf("输入无效，位宽未更改。\n");
        return;
    }
    if (w < 1 || w > 63) {
        printf("位宽超出范围 (1~63)，未更改。\n");
        return;
    }
    bit_width = w;
    printf("位宽已设置为 %d 位\n", bit_width);
    printf("有符号范围: [%lld, %lld]\n", get_min_signed(), get_max_signed());
    printf("无符号范围: [0, %lld]\n", get_max_unsigned());
}

/* ==================== 选项 2：八进制转十进制 ==================== */
static void octal_to_decimal(void) {
    char input[128];
    int is_negative = 0;
    int start = 0;
    long long decimal = 0;
    long long max_val;

    printf("\n===== 八进制 → 十进制 =====\n");
    printf("当前位宽: %d 位\n", bit_width);
    printf("请输入八进制数: ");
    scanf("%s", input);

    /* 处理负号前缀 */
    if (input[0] == '-') {
        is_negative = 1;
        start = 1;
    }

    /* 校验并转换 */
    for (int i = start; input[i] != '\0'; i++) {
        if (input[i] < '0' || input[i] > '7') {
            printf("错误: '%c' 不是有效的八进制字符！\n", input[i]);
            return;
        }
        decimal = decimal * 8 + (input[i] - '0');
    }

    /* 还原负号 */
    if (is_negative) {
        decimal = -decimal;
    }

    /*
     * 补码解释：
     * 将输入的八进制数视为位宽为 bit_width 的二进制补码表示。
     * 如果该值超出了有符号范围 [min, max]，则说明最高有效位为 1，
     * 应将其解释为负数：实际值 = 输入值 - 2^bit_width
     */
    max_val = get_max_signed();
    if (decimal > max_val) {
        decimal -= (1LL << bit_width);
    }

    printf("八进制: %s\n", input);
    printf("十进制: %lld\n", decimal);
}

/* ==================== 选项 3：十进制转八进制 ==================== */
static void decimal_to_octal(void) {
    long long decimal;
    long long unsigned_val;
    char octal[128];
    int idx = 0;
    int i;

    printf("\n===== 十进制 → 八进制 =====\n");
    printf("当前位宽: %d 位\n", bit_width);
    printf("请输入十进制数: ");
    scanf("%lld", &decimal);

    /* 检查范围 */
    if (decimal > get_max_signed() || decimal < get_min_signed()) {
        printf("警告: %lld 超出 %d 位有符号范围 [%lld, %lld]\n",
               decimal, bit_width, get_min_signed(), get_max_signed());
        printf("将自动截断为位宽范围内的补码表示。\n");
    }

    /*
     * 负数补码转换：
     * 将十进制负数映射到位宽对应的无符号补码值。
     * 例如 8 位下 -1 → 255, -128 → 128
     */
    if (decimal < 0) {
        unsigned_val = decimal + (1LL << bit_width);
    } else {
        unsigned_val = decimal;
    }

    /* 对超出位宽的值进行截断 */
    unsigned_val &= get_max_unsigned();

    /* 特殊情况：值为 0 */
    if (unsigned_val == 0) {
        printf("十进制: %lld\n", decimal);
        printf("八进制: 0\n");
        return;
    }

    /* 逐位除以 8 取余，得到八进制表示 */
    long long temp = unsigned_val;
    while (temp > 0) {
        octal[idx++] = (temp % 8) + '0';
        temp /= 8;
    }
    octal[idx] = '\0';

    /* 反转字符串 */
    for (i = 0; i < idx / 2; i++) {
        char c = octal[i];
        octal[i] = octal[idx - 1 - i];
        octal[idx - 1 - i] = c;
    }

    printf("十进制: %lld\n", decimal);
    printf("八进制: %s\n", octal);
}

int main(void){

        int choice;

    printf("================================\n");
    printf("   进制转换工具 (支持负数)\n");
    printf("================================\n");
    printf("默认位宽: %d 位\n", bit_width);
    printf("有符号范围: [%lld, %lld]\n\n", get_min_signed(), get_max_signed());

    while (1) {
        printf("\n---------- 主菜单 ----------\n");
        printf("  1. 选择位宽\n");
        printf("  2. 八进制 → 十进制\n");
        printf("  3. 十进制 → 八进制\n");
        printf("  0. 退出\n");
        printf("请选择 (0-3): ");

        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            printf("输入无效，请重新输入。\n");
            continue;
        }

        switch (choice) {
            case 1: select_bit_width(); break;
            case 2: octal_to_decimal(); break;
            case 3: decimal_to_octal(); break;
            case 0:
                printf("程序已退出。\n");
                return 0;
            default:
                printf("无效选项，请重新输入。\n");
        }
    }

    return 0;

}