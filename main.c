#include <stdio.h>
#include <string.h>

/*
 * 递归：二进制(以整数形式存储)转十进制
 * 前提：binary >= 0 且仅含 0/1 数字
 */
long long binaryToDecimal(long long binary) {
    if (binary < 0) {
        return -1;
    }
    if (binary == 0) {
        return 0;
    }
    long long remainder = binary % 10;
    long long rest      = binary / 10;
    return binaryToDecimal(rest) * 2 + remainder;
}

/*
 * 递归：将十进制数的二进制形式写入缓冲区
 * 前提：decimal >= 0
 */
void decimalToBinaryBuf(long long decimal, char *buf, int *pos) {
    if (decimal == 0) {
        return;
    }
    decimalToBinaryBuf(decimal / 2, buf, pos);
    buf[(*pos)++] = '0' + (int)(decimal % 2);
}

/*
 * 按位宽打印二进制（补前导零，每 4 位加空格）
 * value 为真实数值，函数内部用 /2 计算真实二进制位数
 */
void printBinaryPadded(long long value, int bitWidth) {
    char buf[80];
    int pos = 0;
    memset(buf, 0, sizeof(buf));

    if (value == 0) {
        for (int i = 0; i < bitWidth; i++) {
            buf[pos++] = '0';
        }
    } else {
        long long temp = value;
        int bits = 0;
        while (temp > 0) {
            bits++;
            temp /= 2;
        }
        for (int i = 0; i < bitWidth - bits; i++) {
            buf[pos++] = '0';
        }
        decimalToBinaryBuf(value, buf, &pos);
    }
    buf[pos] = '\0';

    char output[128];
    int outPos = 0;
    int len = pos;
    for (int i = 0; i < len; i++) {
        int remaining = len - i;
        if (i > 0 && remaining % 4 == 0) {
            output[outPos++] = ' ';
        }
        output[outPos++] = buf[i];
    }
    output[outPos] = '\0';
    printf("%s", output);
}

/* 校验二进制数格式合法（仅含 0/1）且不超过位宽的无符号上限 */
int validateBinary(long long binary, int bits) {
    if (binary < 0) {
        return 0;
    }
    long long temp = binary;
    long long maxVal = (1LL << bits) - 1;
    while (temp > 0) {
        int digit = temp % 10;
        if (digit != 0 && digit != 1) {
            return 0;
        }
        temp /= 10;
    }
    long long decimal = binaryToDecimal(binary);
    if (decimal > maxVal) {
        return 0;
    }
    return 1;
}

int main(void) {
    int choice, signChoice, bits;

    printf("===== 进制转换工具 =====\n");
    printf("  1. 二进制 -> 十进制\n");
    printf("  2. 十进制 -> 二进制\n");
    printf("========================\n");

    /* 第一步：选择功能 */
    printf("请选择功能 (1/2): ");
    if (scanf("%d", &choice) != 1) {
        printf("输入无效！\n");
        return 1;
    }
    if (choice != 1 && choice != 2) {
        printf("无效的功能选择！\n");
        return 1;
    }

    /*
     * 第二步：仅二进制→十进制时选择语义
     *   1 = 正数：输入为正数的无符号二进制
     *   2 = 负数（绝对值）：输入为绝对值的二进制，展示原码/反码/补码
     *   3 = 补码解读：输入本身就是补码位模式，直接解析为有符号十进制
     */
    int isNegative = 0;
    int isTwosComp = 0;
    if (choice == 1) {
        printf("请输入的二进制数是 (1=正数 / 2=负数(绝对值) / 3=补码解读): ");
        if (scanf("%d", &signChoice) != 1) {
            printf("输入无效！\n");
            return 1;
        }
        if (signChoice == 1) {
            isNegative = 0;
            isTwosComp = 0;
        } else if (signChoice == 2) {
            isNegative = 1;
            isTwosComp = 0;
        } else if (signChoice == 3) {
            isNegative = 0;
            isTwosComp = 1;
        } else {
            printf("无效的选择！\n");
            return 1;
        }
    }

    /* 第三步：选择位宽 */
    printf("请选择位宽 (3=8位 / 4=16位 / 5=32位): ");
    if (scanf("%d", &bits) != 1) {
        printf("输入无效！\n");
        return 1;
    }

    int bitWidth;
    if (bits == 3)      bitWidth = 8;
    else if (bits == 4) bitWidth = 16;
    else if (bits == 5) bitWidth = 32;
    else {
        printf("无效的位宽选择！\n");
        return 1;
    }

    printf("已选择: %d 位位宽\n\n", bitWidth);

    if (choice == 1) {
        /* ===== 二进制 -> 十进制 ===== */
        long long binary;
        printf("请输入一个二进制数: ");
        if (scanf("%lld", &binary) != 1) {
            printf("输入无效！\n");
            return 1;
        }

        char trailing;
        if (scanf("%c", &trailing) == 1 && trailing != '\n' && trailing != ' ') {
            printf("输入无效！二进制数后包含非法字符 '%c'。\n", trailing);
            return 1;
        }

        if (!validateBinary(binary, bitWidth)) {
            printf("输入无效！请确保只包含 0 和 1，且不超过 %d 位位宽范围。\n", bitWidth);
        } else {
            long long decimal = binaryToDecimal(binary);
            long long signedMax = (1LL << (bitWidth - 1)) - 1;

            if (isTwosComp) {
                /*
                 * 补码解读模式
                 * 输入的二进制直接视为补码位模式
                 * 若最高位为 1 → 负数：实际值 = unsigned_value - 2^n
                 * 若最高位为 0 → 正数：实际值 = unsigned_value
                 */
                long long signBit = 1LL << (bitWidth - 1);
                long long actualValue;

                if (decimal & signBit) {
                    /* 最高位为 1：负数补码 */
                    actualValue = decimal - (1LL << bitWidth);
                } else {
                    /* 最高位为 0：正数 */
                    actualValue = decimal;
                }

                printf("补码解读: ");
                printBinaryPadded(decimal, bitWidth);
                printf(" (补码) = %lld (十进制)\n", actualValue);

                if (actualValue < 0) {
                    /* 负数：额外展示原码和反码 */
                    long long absVal = -actualValue;
                    long long mask     = signBit - 1;
                    long long fullMask = (1LL << bitWidth) - 1;

                    if (absVal == signBit) {
                        /* -2^(n-1)：只有补码 */
                        printf("注: %lld 在原码/反码体系中无法表示，仅有补码。\n", actualValue);
                    } else {
                        long long original = absVal | signBit;
                        long long oneComp  = ((absVal & mask) ^ mask) | signBit;
                        printf("原码: ");
                        printBinaryPadded(original, bitWidth);
                        printf("\n");
                        printf("反码: ");
                        printBinaryPadded(oneComp, bitWidth);
                        printf("\n");
                    }
                }
                printf("补码: ");
                printBinaryPadded(decimal, bitWidth);
                printf("\n");

            } else if (isNegative) {
                /* 负数（绝对值）模式 */
                if (decimal == 0) {
                    printf("0 不能为负数！\n");
                } else if (decimal == signedMax + 1) {
                    printf("-%lld 在原码/反码体系中无法表示（%d 位原码/反码范围为 -%lld ~ %lld）。\n",
                           decimal, bitWidth, signedMax, signedMax);
                    printf("该值只有补码表示: ");
                    printBinaryPadded(1LL << (bitWidth - 1), bitWidth);
                    printf("\n如需完整转换，请使用功能 2（十进制 -> 二进制）输入 %lld。\n",
                           -(signedMax + 1));
                } else if (decimal > signedMax) {
                    printf("绝对值 %lld 超出 %d 位有符号范围（最大 %lld）！\n",
                           decimal, bitWidth, signedMax);
                } else {
                    long long signBit  = 1LL << (bitWidth - 1);
                    long long mask     = signBit - 1;
                    long long fullMask = (1LL << bitWidth) - 1;

                    long long original = decimal | signBit;
                    long long oneComp  = ((decimal & mask) ^ mask) | signBit;
                    long long twoComp  = (oneComp + 1) & fullMask;

                    printf("十进制值: %lld\n", -decimal);
                    printf("原码: ");
                    printBinaryPadded(original, bitWidth);
                    printf("\n");
                    printf("反码: ");
                    printBinaryPadded(oneComp, bitWidth);
                    printf("\n");
                    printf("补码: ");
                    printBinaryPadded(twoComp, bitWidth);
                    printf("\n");
                }
            } else {
                /* 正数模式 */
                if (decimal > signedMax) {
                    printf("正数值 %lld 超出 %d 位有符号正数范围（0 ~ %lld）！\n",
                           decimal, bitWidth, signedMax);
                    printf("提示: 若将该二进制视为补码，它表示负数 %lld。\n",
                           decimal - (1LL << bitWidth));
                    printf("      请选择\"补码解读\"选项进行转换。\n");
                } else {
                    printf("转换结果: ");
                    printBinaryPadded(decimal, bitWidth);
                    printf(" (二进制) = %lld (十进制)\n", decimal);
                    printf("原码: ");
                    printBinaryPadded(decimal, bitWidth);
                    printf("\n");
                    printf("反码: ");
                    printBinaryPadded(decimal, bitWidth);
                    printf("\n");
                    printf("补码: ");
                    printBinaryPadded(decimal, bitWidth);
                    printf("\n");
                }
            }
        }
    } else {
        /* ===== 十进制 -> 二进制 ===== */
        long long decimal;
        long long maxVal = (1LL << (bitWidth - 1)) - 1;
        long long minVal = -(1LL << (bitWidth - 1));

        printf("请输入一个十进制数 (%lld ~ %lld): ", minVal, maxVal);
        if (scanf("%lld", &decimal) != 1) {
            printf("输入无效！\n");
            return 1;
        }

        char trailing;
        if (scanf("%c", &trailing) == 1 && trailing != '\n' && trailing != ' ') {
            printf("输入无效！十进制数后包含非法字符 '%c'。\n", trailing);
            return 1;
        }

        if (decimal < minVal || decimal > maxVal) {
            printf("输入超出 %d 位有符号范围 (%lld ~ %lld)！\n", bitWidth, minVal, maxVal);
        } else {
            long long outputVal = decimal;
            if (decimal < 0) {
                outputVal = (1LL << bitWidth) + decimal;
            }
            printf("转换结果: %lld (十进制) = ", decimal);
            printBinaryPadded(outputVal, bitWidth);
            printf(" (二进制补码)\n");
        }
    }

    return 0;
}