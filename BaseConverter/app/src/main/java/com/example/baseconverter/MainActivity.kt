package com.example.baseconverter

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.widget.Toast
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import java.lang.Float.floatToIntBits
import java.lang.Double.doubleToLongBits
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

    var binText by remember { mutableStateOf("") }
    var decText by remember { mutableStateOf("") }
    var octText by remember { mutableStateOf("") }
    var hexText by remember { mutableStateOf("") }
    var isUpdating by remember { mutableStateOf(false) }
    var showDialog by remember { mutableStateOf(false) }
    var calculationProcess by remember { mutableStateOf("") }

    val context = LocalContext.current

    fun updateAllFromInteger(value: Long, source: String) {
        if (isUpdating) return
        isUpdating = true

        val mask = if (bitWidth == 64) -1L else (1L shl bitWidth) - 1
        val maskedValue = value and mask

        val displayValue = when (interpretMode) {
            0 -> maskedValue
            1 -> -(maskedValue and mask)
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
            octText = java.lang.Long.toOctalString(maskedValue)
        }

        if (source != "HEX") {
            hexText = java.lang.Long.toHexString(maskedValue).uppercase()
        }

        isUpdating = false
    }

    fun updateAllFromFloat(floatValue: Double, source: String) {
        if (isUpdating) return
        isUpdating = true

        if (floatPrecision == 0) {
            val floatVal = floatValue.toFloat()
            val bits = floatToIntBits(floatVal)
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
                octText = java.lang.Long.toOctalString(maskedBits)
            }

            if (source != "HEX") {
                hexText = java.lang.Integer.toHexString(bits).uppercase()
            }
        } else {
            val bits = doubleToLongBits(floatValue)

            if (source != "DEC") decText = floatValue.toString()

            if (source != "BIN") {
                val sb = StringBuilder()
                for (i in 63 downTo 0) {
                    sb.append(if ((bits shr i) and 1L == 1L) '1' else '0')
                }
                binText = sb.toString()
            }

            if (source != "OCT") {
                octText = java.lang.Long.toOctalString(bits)
            }

            if (source != "HEX") {
                hexText = java.lang.Long.toHexString(bits).uppercase()
            }
        }

        isUpdating = false
    }

    fun updateFromBinary(binary: String, source: String) {
        if (isUpdating) return

        if (floatPrecision == 0 && binary.length == 32) {
            var bits = 0
            for (c in binary) {
                bits = (bits shl 1) or (if (c == '1') 1 else 0)
            }
            val floatVal = intBitsToFloat(bits)
            updateAllFromFloat(floatVal.toDouble(), source)
        } else if (floatPrecision == 1 && binary.length == 64) {
            var bits = 0L
            for (c in binary) {
                bits = (bits shl 1) or (if (c == '1') 1L else 0L)
            }
            val doubleVal = longBitsToDouble(bits)
            updateAllFromFloat(doubleVal, source)
        } else {
            val parsed = binary.toLongOrNull(2) ?: 0L
            updateAllFromInteger(parsed, source)
        }
    }

    fun generateCalculationProcess(): String {
        val sb = StringBuilder()
        sb.append("=== 转换计算过程 ===\n\n")
        sb.append("位宽: $bitWidth bit\n")
        sb.append("解读方式: ${listOf("正数", "负数(绝对值)", "补码")[interpretMode]}\n")

        val isFloat = decText.contains('.')

        if (isFloat) {
            sb.append("浮点精度: ${listOf("单精度(32位)", "双精度(64位)")[floatPrecision]}\n")
        }

        sb.append("\n")

        if (decText.isNotEmpty()) {
            sb.append("十进制值: $decText\n")

            if (isFloat) {
                val decimalValue = decText.toDoubleOrNull() ?: 0.0
                sb.append("\n【浮点数转换】\n")

                if (floatPrecision == 0) {
                    val floatVal = decimalValue.toFloat()
                    val bits = floatToIntBits(floatVal)

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

                    val actualExponent = exponent - 127
                    sb.append("\n实际指数: $exponent - 127 = $actualExponent\n")
                    sb.append("十六进制: ${Integer.toHexString(bits).uppercase()}\n")
                    sb.append("八进制: ${java.lang.Long.toOctalString(bits.toLong() and 0xFFFFFFFFL)}\n")
                } else {
                    val bits = doubleToLongBits(decimalValue)

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

                    val actualExponent = exponent - 1023
                    sb.append("\n实际指数: $exponent - 1023 = $actualExponent\n")
                    sb.append("十六进制: ${java.lang.Long.toHexString(bits).uppercase()}\n")
                    sb.append("八进制: ${java.lang.Long.toOctalString(bits)}\n")
                }
            } else {
                val decimalValue = decText.toLongOrNull() ?: 0L
                val mask = if (bitWidth == 64) -1L else (1L shl bitWidth) - 1
                val maskedValue = decimalValue and mask

                sb.append("\n【整数转换】\n")
                sb.append("原始值: $decimalValue\n")
                sb.append("位宽掩码: 0x${mask.toString(16).uppercase()}\n")
                sb.append("掩码后值: $maskedValue\n\n")

                sb.append("二进制转换 ($bitWidth 位):\n")
                var temp = maskedValue
                val binaryDigits = mutableListOf<Int>()
                for (i in 0 until bitWidth) {
                    val bit = (temp and 1L).toInt()
                    binaryDigits.add(bit)
                    temp = temp shr 1
                }
                val binaryStr = binaryDigits.reversed().joinToString("")
                sb.append("$binaryStr\n\n")

                sb.append("位权展开:\n")
                for (i in binaryDigits.indices) {
                    val bit = binaryDigits[binaryDigits.size - 1 - i]
                    if (bit == 1) {
                        sb.append("1 × 2^$i = ${1L shl i}\n")
                    }
                }

                sb.append("\n八进制: ${java.lang.Long.toOctalString(maskedValue)}\n")
                sb.append("十六进制: ${java.lang.Long.toHexString(maskedValue).uppercase()}\n")

                if (interpretMode == 2 && bitWidth < 64) {
                    val isNegative = (maskedValue and (1L shl (bitWidth - 1))) != 0L
                    if (isNegative) {
                        val signedValue = maskedValue or (-1L shl bitWidth)
                        sb.append("\n补码解读:\n")
                        sb.append("最高位为1，表示负数\n")
                        sb.append("补码值: $signedValue\n")
                    }
                }
            }
        } else {
            sb.append("请输入数值以查看计算过程")
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
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text("Base Converter", fontSize = 28.sp, fontWeight = FontWeight.Bold, color = Color.DarkGray)

            IconButton(onClick = { copyToClipboard("https://github.com/TODESENGEL1116/BinaryConverter") }) {
                Text(
                    text = "分享",
                    fontSize = 16.sp,
                    color = Color.Gray
                )
            }
        }

        Spacer(modifier = Modifier.height(24.dp))

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
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
                    modifier = Modifier
                        .fillMaxWidth()
                        .menuAnchor(),
                    shape = RoundedCornerShape(12.dp),
                    textStyle = LocalTextStyle.current.copy(fontSize = 14.sp)
                )
                ExposedDropdownMenu(
                    expanded = bitWidthExpanded,
                    onDismissRequest = { bitWidthExpanded = false }
                ) {
                    listOf(8, 16, 32, 64).forEach { width ->
                        DropdownMenuItem(
                            text = { Text("$width bit") },
                            onClick = {
                                bitWidth = width
                                bitWidthExpanded = false
                                if (decText.isNotEmpty()) {
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
                    modifier = Modifier
                        .fillMaxWidth()
                        .menuAnchor(),
                    shape = RoundedCornerShape(12.dp),
                    textStyle = LocalTextStyle.current.copy(fontSize = 14.sp)
                )
                ExposedDropdownMenu(
                    expanded = interpretExpanded,
                    onDismissRequest = { interpretExpanded = false }
                ) {
                    interpretOptions.forEachIndexed { index, text ->
                        DropdownMenuItem(
                            text = { Text(text) },
                            onClick = {
                                interpretMode = index
                                interpretExpanded = false
                                if (decText.isNotEmpty()) {
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
                    modifier = Modifier
                        .fillMaxWidth()
                        .menuAnchor(),
                    shape = RoundedCornerShape(12.dp),
                    textStyle = LocalTextStyle.current.copy(fontSize = 14.sp)
                )
                ExposedDropdownMenu(
                    expanded = floatExpanded,
                    onDismissRequest = { floatExpanded = false }
                ) {
                    floatOptions.forEachIndexed { index, text ->
                        DropdownMenuItem(
                            text = { Text(text) },
                            onClick = {
                                floatPrecision = index
                                floatExpanded = false
                                if (decText.isNotEmpty()) {
                                    decText.toDoubleOrNull()?.let { updateAllFromFloat(it, "FLOAT_CHANGE") }
                                }
                            }
                        )
                    }
                }
            }
        }

        Spacer(modifier = Modifier.height(32.dp))

        InputRow(
            label = "BIN",
            value = binText,
            onValueChange = {
                binText = it.filter { c -> c == '0' || c == '1' }
                if (it.isNotEmpty()) {
                    updateFromBinary(it, "BIN")
                } else {
                    binText = ""; decText = ""; octText = ""; hexText = ""
                }
            }
        )

        Spacer(modifier = Modifier.height(16.dp))

        InputRow(
            label = "DEC",
            value = decText,
            onValueChange = {
                decText = it.filter { c -> c.isDigit() || c == '-' || c == '.' }
                try {
                    if (it.contains('.')) {
                        val parsed = it.toDoubleOrNull() ?: 0.0
                        updateAllFromFloat(parsed, "DEC")
                    } else {
                        val parsed = it.toLongOrNull() ?: 0L
                        updateAllFromInteger(parsed, "DEC")
                    }
                } catch (e: Exception) {
                    if (it.isEmpty()) {
                        decText = ""; binText = ""; octText = ""; hexText = ""
                    }
                }
            },
            keyboardType = KeyboardType.Decimal
        )

        Spacer(modifier = Modifier.height(16.dp))

        InputRow(
            label = "OCT",
            value = octText,
            onValueChange = {
                octText = it.filter { c -> c in '0'..'7' }
                if (it.isNotEmpty()) {
                    try {
                        val parsed = it.toLongOrNull(8) ?: 0L
                        updateAllFromInteger(parsed, "OCT")
                    } catch (e: Exception) { }
                } else {
                    octText = ""; decText = ""; binText = ""; hexText = ""
                }
            }
        )

        Spacer(modifier = Modifier.height(16.dp))

        InputRow(
            label = "HEX",
            value = hexText,
            onValueChange = {
                hexText = it.filter { c -> c.isDigit() || c in 'A'..'F' || c in 'a'..'f' }.uppercase()
                if (it.isNotEmpty()) {
                    try {
                        val parsed = it.toLongOrNull(16) ?: 0L
                        updateAllFromInteger(parsed, "HEX")
                    } catch (e: Exception) { }
                } else {
                    hexText = ""; decText = ""; binText = ""; octText = ""
                }
            }
        )

        Spacer(modifier = Modifier.height(32.dp))

        // 底部统一的“过程”按钮，外观与上方输入框保持一致
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
                Column(modifier = Modifier.fillMaxWidth().padding(8.dp)) {
                    Text(
                        text = calculationProcess,
                        fontSize = 14.sp,
                        lineHeight = 20.sp
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

@Composable
fun InputRow(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    keyboardType: KeyboardType = KeyboardType.Ascii
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically
    ) {
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
            onValueChange = onValueChange,
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
            keyboardOptions = KeyboardOptions(keyboardType = keyboardType)
        )
    }
}

@Composable
fun ProcessRow(onProcessClick: () -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // 左侧标签，与 BIN/DEC/OCT/HEX 完全一致
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

        // 右侧按钮，外观模仿 TextField 的白色圆角带阴影样式
        Button(
            onClick = onProcessClick,
            modifier = Modifier.weight(1f).height(56.dp),
            shape = RoundedCornerShape(16.dp),
            colors = ButtonDefaults.buttonColors(
                containerColor = Color.White,
                contentColor = Color.Gray
            ),
            elevation = ButtonDefaults.buttonElevation(
                defaultElevation = 4.dp,
                pressedElevation = 2.dp
            )
        ) {
            Text(
                text = "查看转换计算过程",
                fontSize = 18.sp,
                fontWeight = FontWeight.Medium
            )
        }
    }
}