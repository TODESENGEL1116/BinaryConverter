package com.example.baseconverter

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.widget.Toast
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import java.lang.Float.floatToRawIntBits
import java.lang.Double.doubleToRawLongBits
import java.lang.Float.intBitsToFloat
import java.lang.Double.longBitsToDouble

@OptIn(ExperimentalMaterial3Api::class)
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme(colorScheme = lightColorScheme()) {
                Surface(modifier = Modifier.fillMaxSize(), color = Color(0xFFF5F5F5)) {
                    BaseConverterScreen()
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun BaseConverterScreen() {
    var bitWidth by remember { mutableIntStateOf(8) }
    var interpretMode by remember { mutableIntStateOf(0) }
    var floatPrecision by remember { mutableIntStateOf(0) }
    // 显式状态管理
    var isFloatMode by remember { mutableStateOf(false) }
    var lastSource by remember { mutableStateOf("") }
    var binText by remember { mutableStateOf("") }
    var decText by remember { mutableStateOf("") }
    var octText by remember { mutableStateOf("") }
    var hexText by remember { mutableStateOf("") }
    var isUpdating by remember { mutableStateOf(false) }
    var showDialog by remember { mutableStateOf(false) }
    var calculationProcess by remember { mutableStateOf("") }

    val context = LocalContext.current

    // 按 radix 解析为 64 位位模式
    // toLongOrNull 只支持有符号范围, "FFFFFFFFFFFFFFFF"/64位全1二进制会返回 null;
    // 手工累加依赖 Long 乘加的模 2^64 溢出回绕, 恰好得到正确的位模式
    fun parseRadixUnsigned(s: String, radix: Int): Long? {
        var v = 0L
        for (c in s) {
            val d = Character.digit(c, radix)
            if (d < 0) return null
            v = v * radix + d
        }
        return v
    }

    // ★ 升级: 根据位宽和解读方式计算四个进制的取值范围提示
    // 返回 (bin提示, dec提示, oct提示, hex提示)
    fun rangeHints(): List<String> {
        val maxUnsigned = if (bitWidth == 64) "18446744073709551615"
        else ((1L shl bitWidth) - 1).toString()
        val binHint = "范围 0 ~ " + "1".repeat(bitWidth)
        val octHint = if (bitWidth == 64) "范围 0 ~ " + "7".repeat(22)
        else "范围 0 ~ " + java.lang.Long.toOctalString((1L shl bitWidth) - 1)
        val hexHint = if (bitWidth == 64) "范围 0 ~ " + "F".repeat(16)
        else "范围 0 ~ " + java.lang.Long.toHexString((1L shl bitWidth) - 1).uppercase()
        val decHint = when (interpretMode) {
            0 -> "范围 0 ~ $maxUnsigned"
            1 -> "范围 -$maxUnsigned ~ 0"
            2 -> if (bitWidth == 64) "范围 -9223372036854775808 ~ 9223372036854775807"
            else "范围 ${-(1L shl (bitWidth - 1))} ~ ${(1L shl (bitWidth - 1)) - 1}"
            else -> ""
        }
        return listOf(binHint, decHint, octHint, hexHint)
    }
    val hints = rangeHints()

    // 集中管理 isFloatMode 和 lastSource
    fun updateAllFromInteger(value: Long, source: String) {
        if (isUpdating) return
        isFloatMode = false
        lastSource = source
        isUpdating = true
        try {
            val mask = if (bitWidth == 64) -1L else (1L shl bitWidth) - 1
            val maskedValue = value and mask
            val displayValue = when (interpretMode) {
                0 -> maskedValue
                // 64 位下位模式无符号值超过 Long.MAX_VALUE 时, 取负不可用 Long 表示,
                // (全1模式原来会回绕显示成错误的正数 1), 用 BigInteger 正确计算
                1 -> {
                    if (bitWidth == 64 && maskedValue < 0) {
                        java.math.BigInteger.valueOf(maskedValue)
                            .add(java.math.BigInteger("18446744073709551616"))
                            .negate().toString()
                    } else -(maskedValue and mask)
                }
                2 -> {
                    if (bitWidth < 64 && (maskedValue and (1L shl (bitWidth - 1))) != 0L) {
                        maskedValue or (-1L shl bitWidth)
                    } else maskedValue
                }
                else -> maskedValue
            }
            if (source != "DEC") decText = displayValue.toString()
            if (source != "BIN") {
                val sb = StringBuilder()
                var temp = maskedValue
                for (i in 0 until bitWidth) {
                    sb.append(if (temp and 1L == 1L) '1' else '0')
                    temp = temp shr 1
                }
                binText = sb.reverse().toString()
            }
            if (source != "OCT") {
                val octLen = (bitWidth + 2) / 3
                octText = java.lang.Long.toOctalString(maskedValue).padStart(octLen, '0')
            }
            if (source != "HEX") {
                val hexLen = (bitWidth + 3) / 4
                hexText = java.lang.Long.toHexString(maskedValue).uppercase().padStart(hexLen, '0')
            }
        } finally {
            isUpdating = false
        }
    }

    // 集中管理 isFloatMode 和 lastSource
    fun updateAllFromFloat(floatValue: Double, source: String) {
        if (isUpdating) return
        isFloatMode = true
        lastSource = source
        isUpdating = true
        try {
            if (floatPrecision == 0) {
                val floatVal = floatValue.toFloat()
                val bits = floatToRawIntBits(floatVal)
                val maskedBits = bits.toLong() and 0xFFFFFFFFL
                if (source != "DEC") decText = floatVal.toString()
                if (source != "BIN") {
                    val sb = StringBuilder()
                    for (i in 31 downTo 0) {
                        sb.append(if ((bits shr i) and 1 == 1) '1' else '0')
                    }
                    binText = sb.toString()
                }
                if (source != "OCT") {
                    octText = java.lang.Long.toOctalString(maskedBits).padStart(11, '0')
                }
                if (source != "HEX") {
                    hexText = java.lang.Integer.toHexString(bits).uppercase().padStart(8, '0')
                }
            } else {
                val bits = doubleToRawLongBits(floatValue)
                if (source != "DEC") decText = floatValue.toString()
                if (source != "BIN") {
                    val sb = StringBuilder()
                    for (i in 63 downTo 0) {
                        sb.append(if ((bits shr i) and 1L == 1L) '1' else '0')
                    }
                    binText = sb.toString()
                }
                if (source != "OCT") {
                    octText = java.lang.Long.toOctalString(bits).padStart(22, '0')
                }
                if (source != "HEX") {
                    hexText = java.lang.Long.toHexString(bits).uppercase().padStart(16, '0')
                }
            }
        } finally {
            isUpdating = false
        }
    }

    // 浮点模式 BIN 超长截断; 整数模式超长回写当前框, 显示与解析值一致
    fun updateFromBinary(binary: String, source: String) {
        if (isUpdating) return
        if (binary.isEmpty()) {
            binText = ""; decText = ""; octText = ""; hexText = ""
            lastSource = ""
            isFloatMode = false
            return
        }
        if (isFloatMode) {
            val need = if (floatPrecision == 0) 32 else 64
            // 超长截取低 need 位 (与 HEX 行为一致), 不足时保持旧值不闪烁
            val over = binary.length > need
            val eff = if (over) binary.takeLast(need) else binary
            if (eff.length != need) {
                return   // 仍在输入中, 不触发其他字段更新
            }
            if (over) {
                Toast.makeText(context, "已截取低 $need 位", Toast.LENGTH_SHORT).show()
            }
            binText = eff   // 显示与解析值保持一致
            if (floatPrecision == 0) {
                var bits = 0
                for (c in eff) {
                    bits = (bits shl 1) or (if (c == '1') 1 else 0)
                }
                updateAllFromFloat(intBitsToFloat(bits).toDouble(), source)
            } else {
                var bits = 0L
                for (c in eff) {
                    bits = (bits shl 1) or (if (c == '1') 1L else 0L)
                }
                updateAllFromFloat(longBitsToDouble(bits), source)
            }
            return
        }
        // 整数模式：超长截取，解析失败保留旧值
        val over = binary.length > bitWidth
        val trimmed = if (over) binary.takeLast(bitWidth) else binary
        // 64 位高位为 1 的二进制 toLongOrNull 会返回 null, 改用无符号解析
        val parsed = parseRadixUnsigned(trimmed, 2)
        if (parsed != null) {
            updateAllFromInteger(parsed, source)
            if (over) {
                binText = trimmed   // 回写当前框, 显示与实际解析的位模式一致
                Toast.makeText(context, "已截取低 $bitWidth 位", Toast.LENGTH_SHORT).show()
            }
        }
    }

    fun generateCalculationProcess(): String {
        // 中间态/空输入守卫, 避免 "-"/"1e" 触发按 0 的全套推导
        if (decText.isEmpty()) return "请输入数值以查看计算过程"
        val midState = decText == "-" || decText == "." || decText == "-." ||
                decText.endsWith("e", true) || decText.endsWith("e+", true) ||
                decText.endsWith("e-", true)
        if (midState) return "输入未完成，请补全数值后查看转换过程"
        if (isFloatMode && decText.toDoubleOrNull() == null)
            return "输入未完成，请补全数值后查看转换过程"

        val sb = StringBuilder()
        sb.append("=== 转换计算过程 ===\n\n")
        sb.append("位宽: $bitWidth bit\n")
        sb.append("解读方式: ${listOf("正数", "负数(绝对值)", "补码")[interpretMode]}\n")
        if (isFloatMode) {
            sb.append("浮点精度: ${listOf("单精度(32位)", "双精度(64位)")[floatPrecision]}\n")
        }
        sb.append("\n")

        if (decText.isNotEmpty()) {
            sb.append("十进制值: $decText\n")
            if (isFloatMode) {
                val decimalValue = decText.toDoubleOrNull() ?: 0.0
                sb.append("\n【浮点数转换】\n")
                if (floatPrecision == 0) {
                    val floatVal = decimalValue.toFloat()
                    val bits = floatToRawIntBits(floatVal)
                    sb.append("单精度(32位) IEEE 754:\n")
                    sb.append("数值: $floatVal\n")
                    sb.append("二进制: ${Integer.toBinaryString(bits).padStart(32, '0')}\n")
                    val sign = (bits shr 31) and 1
                    val exponent = (bits shr 23) and 0xFF
                    val mantissa = bits and 0x7FFFFF
                    sb.append("\n分解:\n")
                    sb.append("符号位(S): $sign ${if (sign == 0) "(正数)" else "(负数)"}\n")
                    sb.append("指数位(E): ${Integer.toBinaryString(exponent).padStart(8, '0')} = $exponent\n")
                    sb.append("尾数位(M): ${Integer.toBinaryString(mantissa).padStart(23, '0')}\n")
                    if (exponent == 0xFF) {
                        sb.append("\n特殊值: ${if (mantissa == 0) "Infinity (无穷大)" else "NaN (非数)"}\n")
                    } else if (exponent == 0) {
                        sb.append("\n特殊值: ${if (mantissa == 0) "0.0" else "非规格化数 (Denormalized)"}\n")
                    } else {
                        val actualExponent = exponent - 127
                        sb.append("\n实际指数: $exponent - 127 = $actualExponent\n")
                    }
                    sb.append("十六进制: ${Integer.toHexString(bits).uppercase()}\n")
                    sb.append("八进制: ${java.lang.Long.toOctalString(bits.toLong() and 0xFFFFFFFFL)}\n")
                } else {
                    val bits = doubleToRawLongBits(decimalValue)
                    sb.append("双精度(64位) IEEE 754:\n")
                    sb.append("数值: $decimalValue\n")
                    val binaryStr = StringBuilder()
                    for (i in 63 downTo 0) {
                        binaryStr.append(if ((bits shr i) and 1L == 1L) '1' else '0')
                        if (i == 52 || i == 62) binaryStr.append(" ")
                    }
                    sb.append("二进制: $binaryStr\n")
                    val sign = (bits shr 63) and 1L
                    val exponent = (bits shr 52) and 0x7FFL
                    val mantissa = bits and 0xFFFFFFFFFFFFFL
                    sb.append("\n分解:\n")
                    sb.append("符号位(S): $sign ${if (sign == 0L) "(正数)" else "(负数)"}\n")
                    sb.append("指数位(E): ${java.lang.Long.toBinaryString(exponent).padStart(11, '0')} = $exponent\n")
                    sb.append("尾数位(M): ${java.lang.Long.toBinaryString(mantissa).padStart(52, '0')}\n")
                    if (exponent == 0x7FFL) {
                        sb.append("\n特殊值: ${if (mantissa == 0L) "Infinity (无穷大)" else "NaN (非数)"}\n")
                    } else if (exponent == 0L) {
                        sb.append("\n特殊值: ${if (mantissa == 0L) "0.0" else "非规格化数 (Denormalized)"}\n")
                    } else {
                        val actualExponent = exponent - 1023
                        sb.append("\n实际指数: $exponent - 1023 = $actualExponent\n")
                    }
                    sb.append("十六进制: ${java.lang.Long.toHexString(bits).uppercase()}\n")
                    sb.append("八进制: ${java.lang.Long.toOctalString(bits)}\n")
                }
            } else {
                val decimalValue = decText.toLongOrNull() ?: 0L
                val mask = if (bitWidth == 64) -1L else (1L shl bitWidth) - 1
                val maskedValue = decimalValue and mask
                sb.append("\n【整数转换】\n")
                sb.append("原始值: $decimalValue\n")
                // 64 位下 mask=-1L, Long.toString(16) 会输出 "0x-1",
                // 改用 Long.toHexString 输出 "0xFFFFFFFFFFFFFFFF"
                sb.append("位宽掩码: 0x${java.lang.Long.toHexString(mask).uppercase()}\n")
                // 掩码后值统一按无符号显示, 与 8 位时显示 251 的风格一致
                sb.append("掩码后值: ${java.lang.Long.toUnsignedString(maskedValue)}\n\n")

                sb.append("二进制转换 ($bitWidth 位):\n")
                var temp = maskedValue
                val binaryDigits = mutableListOf<Int>()   // LSB 在前: binaryDigits[i] 的权值是 2^i
                for (i in 0 until bitWidth) {
                    val bit = (temp and 1L).toInt()
                    binaryDigits.add(bit)
                    temp = temp shr 1
                }
                val binaryStr = binaryDigits.reversed().joinToString("")
                sb.append("$binaryStr\n\n")

                // 位权展开: binaryDigits[i] 的权值就是 2^i, 每项独占一行
                sb.append("位权展开 (每位一行):\n")
                var sum = 0L
                var hasTerms = false
                for (i in binaryDigits.indices) {
                    val bit = binaryDigits[i]
                    if (bit == 1) {
                        val termStr = if (i == 63) "9223372036854775808" else (1L shl i).toString()
                        sb.append("1 × 2^$i = $termStr\n")
                        sum += (1L shl i)   // Long 模 2^64 加法, 累加和按无符号解读即为正确值
                        hasTerms = true
                    }
                }
                if (!hasTerms) {
                    sb.append("0 × 2^0 = 0\n")
                    sum = 0
                }
                val sumStr = if (sum < 0) java.lang.Long.toUnsignedString(sum) else sum.toString()
                sb.append("位模式无符号求和 = $sumStr\n")

                // 补码模式且为负数时, 追加补码权值展开 (符号位权值取负), 验证回到十进制结果
                val isNegative = if (bitWidth == 64) (maskedValue < 0)
                else ((maskedValue and (1L shl (bitWidth - 1))) != 0L)
                if (interpretMode == 2 && isNegative) {
                    sb.append("\n补码权值展开 (符号位权值取负):\n")
                    var csum = 0L
                    if (bitWidth == 64) {
                        sb.append("-1 × 2^63 = -9223372036854775808\n")
                        csum += (1L shl 63)   // Long.MIN_VALUE, 模 2^64 语义下即负的 2^63
                    } else {
                        val signTerm = -(1L shl (bitWidth - 1))
                        sb.append("-1 × 2^${bitWidth - 1} = $signTerm\n")
                        csum += signTerm
                    }
                    for (i in 0 until bitWidth - 1) {
                        if (binaryDigits[i] == 1) {
                            val termStr = if (i == 63) "9223372036854775808" else (1L shl i).toString()
                            sb.append("1 × 2^$i = $termStr\n")
                            csum += (1L shl i)
                        }
                    }
                    val signedValue = if (bitWidth < 64) maskedValue or (-1L shl bitWidth) else maskedValue
                    sb.append("求和 = $csum  (与十进制值 $signedValue 一致)\n")
                }
                sb.append("\n")

                sb.append("八进制转换:\n")
                sb.append("方法：从右到左每3位二进制分为一组\n")
                val binaryGroups = mutableListOf<String>()
                for (i in binaryStr.length - 1 downTo 0 step 3) {
                    val start = maxOf(0, i - 2)
                    val group = binaryStr.substring(start, i + 1)
                    binaryGroups.add(0, group)
                }
                sb.append("分组：${binaryGroups.joinToString(" ")}\n")
                sb.append("转换：")
                var octResult = ""
                binaryGroups.forEachIndexed { index, group ->
                    val octDigit = group.padStart(3, '0').toInt(2)
                    sb.append("$group = $octDigit")
                    if (index < binaryGroups.size - 1) sb.append(", ")
                    octResult += octDigit.toString()
                }
                octResult = octResult.trimStart('0').ifEmpty { "0" }
                sb.append("\n结果：$octResult\n\n")

                sb.append("十六进制转换:\n")
                sb.append("方法：从右到左每4位二进制分为一组\n")
                val hexGroups = mutableListOf<String>()
                for (i in binaryStr.length - 1 downTo 0 step 4) {
                    val start = maxOf(0, i - 3)
                    val group = binaryStr.substring(start, i + 1)
                    hexGroups.add(0, group)
                }
                sb.append("分组：${hexGroups.joinToString(" ")}\n")
                sb.append("转换：")
                var hexResult = ""
                hexGroups.forEachIndexed { index, group ->
                    val hexDigit = group.padStart(4, '0').toInt(2)
                    val hexChar = when (hexDigit) {
                        10 -> "A"; 11 -> "B"; 12 -> "C"; 13 -> "D"; 14 -> "E"; 15 -> "F"
                        else -> hexDigit.toString()
                    }
                    sb.append("$group = $hexChar")
                    if (index < hexGroups.size - 1) sb.append(", ")
                    hexResult += hexChar
                }
                hexResult = hexResult.trimStart('0').ifEmpty { "0" }
                sb.append("\n结果：$hexResult\n")

                if (interpretMode == 2) {
                    if (isNegative) {
                        sb.append("\n补码解读:\n")
                        sb.append("最高位为1，表示负数\n")
                        val invertedValue = maskedValue.inv() and mask
                        val originalValue = invertedValue + 1
                        if (bitWidth == 64) {
                            sb.append("取反：${java.lang.Long.toBinaryString(invertedValue)}\n")
                            sb.append("加1：${java.lang.Long.toBinaryString(originalValue)}\n")
                        } else {
                            sb.append("取反：${Integer.toBinaryString(invertedValue.toInt()).padStart(bitWidth, '0')}\n")
                            sb.append("加1：${Integer.toBinaryString(originalValue.toInt()).padStart(bitWidth, '0')}\n")
                        }
                        val absStr = if (originalValue < 0 && bitWidth == 64)
                            java.lang.Long.toUnsignedString(originalValue) else originalValue.toString()
                        sb.append("绝对值：$absStr\n")
                        sb.append("最终值：-$absStr\n")
                    } else {
                        sb.append("\n补码解读:\n")
                        sb.append("最高位为0，表示正数\n")
                        val valStr = if (maskedValue < 0 && bitWidth == 64)
                            java.lang.Long.toUnsignedString(maskedValue) else maskedValue.toString()
                        sb.append("正数的补码等于其本身：$valStr\n")
                    }
                } else if (interpretMode == 1 && lastSource != "DEC") {
                    sb.append("\n负数（绝对值）解读:\n")
                    val mvStr = if (maskedValue < 0)
                        java.lang.Long.toUnsignedString(maskedValue) else maskedValue.toString()
                    sb.append("输入的位模式按绝对值看待，取负：-$mvStr\n")
                }
            }
        }
        return sb.toString()
    }

    fun copyToClipboard(text: String) {
        val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val clip = ClipData.newPlainText("GitHub Link", text)
        clipboard.setPrimaryClip(clip)
        Toast.makeText(context, "GitHub链接已复制到剪贴板", Toast.LENGTH_SHORT).show()
    }

    Column(
        modifier = Modifier.fillMaxSize().padding(24.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text("Base Converter", fontSize = 28.sp, fontWeight = FontWeight.Bold, color = Color.DarkGray)
            IconButton(onClick = {
                copyToClipboard("https://github.com/TODESENGEL1116/BinaryConverter")
            }) {
                Text(text = "分享", fontSize = 16.sp, color = Color.Gray)
            }
        }

        Spacer(modifier = Modifier.height(24.dp))

        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            var bitWidthExpanded by remember { mutableStateOf(false) }
            ExposedDropdownMenuBox(
                expanded = bitWidthExpanded,
                onExpandedChange = { bitWidthExpanded = !bitWidthExpanded },
                modifier = Modifier.weight(1f)
            ) {
                OutlinedTextField(
                    value = "$bitWidth bit",
                    onValueChange = {},
                    readOnly = true,
                    modifier = Modifier.fillMaxWidth().menuAnchor(),
                    shape = RoundedCornerShape(12.dp),
                    textStyle = LocalTextStyle.current.copy(fontSize = 14.sp)
                )
                ExposedDropdownMenu(expanded = bitWidthExpanded, onDismissRequest = { bitWidthExpanded = false }) {
                    listOf(8, 16, 32, 64).forEach { width ->
                        DropdownMenuItem(
                            text = { Text("$width bit") },
                            onClick = {
                                bitWidth = width; bitWidthExpanded = false
                                if (decText.isNotEmpty() && !isFloatMode) {
                                    decText.toLongOrNull()?.let { updateAllFromInteger(it, "WIDTH_CHANGE") }
                                }
                            }
                        )
                    }
                }
            }

            var interpretExpanded by remember { mutableStateOf(false) }
            val interpretOptions = listOf("正数", "负数(绝对值)", "补码")
            ExposedDropdownMenuBox(
                expanded = interpretExpanded,
                onExpandedChange = { interpretExpanded = !interpretExpanded },
                modifier = Modifier.weight(1f)
            ) {
                OutlinedTextField(
                    value = interpretOptions[interpretMode],
                    onValueChange = {},
                    readOnly = true,
                    modifier = Modifier.fillMaxWidth().menuAnchor(),
                    shape = RoundedCornerShape(12.dp),
                    textStyle = LocalTextStyle.current.copy(fontSize = 14.sp)
                )
                ExposedDropdownMenu(expanded = interpretExpanded, onDismissRequest = { interpretExpanded = false }) {
                    interpretOptions.forEachIndexed { index, text ->
                        DropdownMenuItem(
                            text = { Text(text) },
                            onClick = {
                                interpretMode = index; interpretExpanded = false
                                if (decText.isNotEmpty() && !isFloatMode) {
                                    decText.toLongOrNull()?.let { updateAllFromInteger(it, "MODE_CHANGE") }
                                }
                            }
                        )
                    }
                }
            }

            var floatExpanded by remember { mutableStateOf(false) }
            val floatOptions = listOf("单精度", "双精度")
            ExposedDropdownMenuBox(
                expanded = floatExpanded,
                onExpandedChange = { floatExpanded = !floatExpanded },
                modifier = Modifier.weight(1f)
            ) {
                OutlinedTextField(
                    value = floatOptions[floatPrecision],
                    onValueChange = {},
                    readOnly = true,
                    modifier = Modifier.fillMaxWidth().menuAnchor(),
                    shape = RoundedCornerShape(12.dp),
                    textStyle = LocalTextStyle.current.copy(fontSize = 14.sp)
                )
                ExposedDropdownMenu(expanded = floatExpanded, onDismissRequest = { floatExpanded = false }) {
                    floatOptions.forEachIndexed { index, text ->
                        DropdownMenuItem(
                            text = { Text(text) },
                            onClick = {
                                floatPrecision = index; floatExpanded = false
                                if (decText.isNotEmpty() && isFloatMode) {
                                    decText.toDoubleOrNull()?.let { updateAllFromFloat(it, "FLOAT_CHANGE") }
                                }
                            }
                        )
                    }
                }
            }
        }

        Spacer(modifier = Modifier.height(32.dp))

        // ★ 升级: 四个输入框均传入取值范围提示, 输入内容后 placeholder 自动消失
        InputRow(
            label = "BIN",
            value = binText,
            onValueChange = {
                val filtered = it.filter { c -> c == '0' || c == '1' }
                binText = filtered
                if (filtered.isNotEmpty()) updateFromBinary(filtered, "BIN")
                else {
                    binText = ""; decText = ""; octText = ""; hexText = ""
                    lastSource = ""
                    isFloatMode = false
                }
            },
            placeholder = hints[0]
        )

        Spacer(modifier = Modifier.height(16.dp))

        InputRow(
            label = "DEC",
            value = decText,
            onValueChange = {
                // 允许 NaN/Infinity 相关字母通过过滤器, 便于编辑浮点特殊值回显
                val filtered = it.filter { c ->
                    c.isDigit() || c == '-' || c == '.' || c == 'e' || c == 'E' || c == '+' ||
                            c in "NaIfity"
                }
                val hasLetters = filtered.any { ch -> ch.isLetter() }
                if (hasLetters) {
                    // 仅当是特殊值的(前)缀时保留显示; 其余字母组合拒绝, 保留旧值
                    val specials = listOf("-Infinity", "Infinity", "NaN")
                    val ok = specials.any { p ->
                        p.startsWith(filtered, ignoreCase = true) ||
                                filtered.startsWith(p, ignoreCase = true)
                    }
                    if (ok) decText = filtered   // 特殊值: 仅保留显示, 不触发解析
                    return@InputRow
                }
                val minusCount = filtered.count { c -> c == '-' }
                val plusCount = filtered.count { c -> c == '+' }
                val dotCount = filtered.count { c -> c == '.' }
                val signOk = (minusCount <= 1 && plusCount <= 1) &&
                        (minusCount == 0 || filtered.indexOf('-') == 0 ||
                                (filtered.indexOf('-') > 0 &&
                                        (filtered[filtered.indexOf('-') - 1] == 'e' ||
                                                filtered[filtered.indexOf('-') - 1] == 'E'))) &&
                        (plusCount == 0 || filtered.indexOf('+') > 0 &&
                                (filtered[filtered.indexOf('+') - 1] == 'e' ||
                                        filtered[filtered.indexOf('+') - 1] == 'E'))
                val isValid = signOk && dotCount <= 1
                if (!isValid && it.isNotEmpty()) return@InputRow

                // DEC 清空时同步清空所有字段并复位模式, 与其他框行为一致
                if (filtered.isEmpty()) {
                    decText = ""; binText = ""; octText = ""; hexText = ""
                    lastSource = ""
                    isFloatMode = false
                    return@InputRow
                }

                decText = filtered
                // 中间态不触发解析，防止闪烁
                if (filtered == "-" || filtered == "." || filtered == "-." ||
                    filtered.endsWith("e") || filtered.endsWith("E") ||
                    filtered.endsWith("e+") || filtered.endsWith("e-") ||
                    filtered.endsWith("E+") || filtered.endsWith("E-")
                ) {
                    return@InputRow
                }
                try {
                    if (filtered.contains('.') || filtered.contains('e', ignoreCase = true)) {
                        val parsed = filtered.toDoubleOrNull()
                        if (parsed != null) updateAllFromFloat(parsed, "DEC")
                    } else {
                        val parsed = filtered.toLongOrNull()
                        if (parsed != null) updateAllFromInteger(parsed, "DEC")
                    }
                } catch (e: Exception) {
                }
            },
            keyboardType = KeyboardType.Decimal,
            placeholder = if (isFloatMode) "任意浮点数" else hints[1]
        )

        Spacer(modifier = Modifier.height(16.dp))

        InputRow(
            label = "OCT",
            value = octText,
            onValueChange = {
                val filtered = it.filter { c -> c in '0'..'7' }
                octText = filtered
                if (filtered.isNotEmpty()) {
                    try {
                        if (isFloatMode) {
                            // 浮点模式下八进制不参与反向解析（该框只读, 此分支为保险）
                        } else {
                            val octLen = (bitWidth + 2) / 3
                            val over = filtered.length > octLen
                            val trimmed = if (over) filtered.takeLast(octLen) else filtered
                            // 高位八进制可能超出有符号范围, 改用无符号解析
                            val trimmedParsed = parseRadixUnsigned(trimmed, 8)
                            if (trimmedParsed != null) {
                                updateAllFromInteger(trimmedParsed, "OCT")
                                if (over) {
                                    octText = trimmed
                                    Toast.makeText(context, "已截取低 $bitWidth 位", Toast.LENGTH_SHORT).show()
                                }
                            }
                        }
                    } catch (e: Exception) {
                    }
                } else {
                    octText = ""; decText = ""; binText = ""; hexText = ""
                    lastSource = ""
                    isFloatMode = false
                }
            },
            readOnly = isFloatMode,
            placeholder = hints[2]
        )

        Spacer(modifier = Modifier.height(16.dp))

        InputRow(
            label = "HEX",
            value = hexText,
            onValueChange = {
                val filtered = it.filter { c ->
                    c.isDigit() || c in 'A'..'F' || c in 'a'..'f'
                }.uppercase()
                if (filtered.isNotEmpty()) {
                    try {
                        // 浮点模式 HEX 长度校验
                        if (isFloatMode) {
                            val needHex = if (floatPrecision == 0) 8 else 16
                            val eff = if (filtered.length > needHex) filtered.takeLast(needHex) else filtered
                            hexText = eff
                            if (eff.length != needHex) return@InputRow   // 位数不够，不解析
                            // 16 位 hex (如 C023300000000000) 超出 Long 有符号范围,
                            // toLongOrNull 返回 null, 双精度负浮点的 HEX 输入会静默失效, 改用无符号解析
                            val parsed = parseRadixUnsigned(eff, 16) ?: return@InputRow
                            if (floatPrecision == 0) {
                                updateAllFromFloat(intBitsToFloat(parsed.toInt()).toDouble(), "HEX")
                            } else {
                                updateAllFromFloat(longBitsToDouble(parsed), "HEX")
                            }
                        } else {
                            hexText = filtered
                            val hexLen = (bitWidth + 3) / 4
                            val over = filtered.length > hexLen
                            val trimmed = if (over) filtered.takeLast(hexLen) else filtered
                            // 64 位高位为 1 的十六进制 toLongOrNull 返回 null, 改用无符号解析
                            val trimmedParsed = parseRadixUnsigned(trimmed, 16)
                            if (trimmedParsed != null) {
                                updateAllFromInteger(trimmedParsed, "HEX")
                                if (over) {
                                    hexText = trimmed
                                    Toast.makeText(context, "已截取低 $bitWidth 位", Toast.LENGTH_SHORT).show()
                                }
                            }
                        }
                    } catch (e: Exception) {
                    }
                } else {
                    hexText = ""; decText = ""; binText = ""; octText = ""
                    lastSource = ""
                    isFloatMode = false
                }
            },
            placeholder = if (isFloatMode) {
                if (floatPrecision == 0) "8位位模式, 如 411A0000" else "16位位模式, 如 C023300000000000"
            } else hints[3]
        )

        Spacer(modifier = Modifier.height(32.dp))

        ProcessRow(
            onProcessClick = {
                calculationProcess = generateCalculationProcess()
                showDialog = true
            }
        )
    }

    if (showDialog) {
        AlertDialog(
            onDismissRequest = { showDialog = false },
            title = { Text("转换计算过程") },
            text = {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(max = 500.dp)
                        .verticalScroll(rememberScrollState())
                        .padding(8.dp)
                ) {
                    Text(
                        text = calculationProcess,
                        fontSize = 14.sp,
                        lineHeight = 20.sp,
                        fontFamily = FontFamily.Monospace
                    )
                }
            },
            confirmButton = {
                TextButton(onClick = { showDialog = false }) {
                    Text("关闭")
                }
            }
        )
    }
}

// ★ 升级: InputRow 增加 placeholder 参数, 框为空时显示取值范围, 输入后自动消失
@Composable
fun InputRow(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    keyboardType: KeyboardType = KeyboardType.Ascii,
    readOnly: Boolean = false,
    placeholder: String = ""
) {
    Row(modifier = Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Surface(
            modifier = Modifier.width(80.dp).height(56.dp),
            shape = RoundedCornerShape(16.dp),
            color = Color.White,
            shadowElevation = 4.dp
        ) {
            Box(contentAlignment = Alignment.Center) {
                Text(text = label, fontSize = 18.sp, fontWeight = FontWeight.Medium, color = Color.Gray)
            }
        }
        Spacer(modifier = Modifier.width(16.dp))
        TextField(
            value = value,
            onValueChange = if (readOnly) { {} } else onValueChange,
            readOnly = readOnly,
            modifier = Modifier.weight(1f).height(56.dp),
            shape = RoundedCornerShape(16.dp),
            colors = TextFieldDefaults.colors(
                focusedContainerColor = Color.White,
                unfocusedContainerColor = Color.White,
                disabledContainerColor = Color.White,
                focusedIndicatorColor = Color.Transparent,
                unfocusedIndicatorColor = Color.Transparent
            ),
            textStyle = LocalTextStyle.current.copy(fontSize = 18.sp, color = Color.DarkGray),
            keyboardOptions = KeyboardOptions(keyboardType = keyboardType),
            placeholder = {
                Text(
                    text = placeholder,
                    fontSize = 13.sp,
                    color = Color.LightGray,
                    maxLines = 1
                )
            }
        )
    }
}

@Composable
fun ProcessRow(onProcessClick: () -> Unit) {
    Row(modifier = Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Surface(
            modifier = Modifier.width(80.dp).height(56.dp),
            shape = RoundedCornerShape(16.dp),
            color = Color.White,
            shadowElevation = 4.dp
        ) {
            Box(contentAlignment = Alignment.Center) {
                Text(text = "过程", fontSize = 18.sp, fontWeight = FontWeight.Medium, color = Color.Gray)
            }
        }
        Spacer(modifier = Modifier.width(16.dp))
        Button(
            onClick = onProcessClick,
            modifier = Modifier.weight(1f).height(56.dp),
            shape = RoundedCornerShape(16.dp),
            colors = ButtonDefaults.buttonColors(containerColor = Color.White, contentColor = Color.Gray),
            elevation = ButtonDefaults.buttonElevation(defaultElevation = 4.dp, pressedElevation = 2.dp)
        ) {
            Text(text = "查看转换计算过程", fontSize = 18.sp, fontWeight = FontWeight.Medium)
        }
    }
}
