#include "OLED.h"
#include "OLED_Font.h"
#include "ti_msp_dl_config.h"
#include <string.h>

/*
 * 硬件 I2C 的 7 位从机地址。
 * 原本软件 I2C 写的 0x78 包含了最低位的读写位(0)，
 * 硬件外设要求传入 7 位地址，即 0x78 >> 1 = 0x3C。
 * 0.96" SSD1306 模块绝大多数是 0x3C，少数是 0x3D
 * —— OLED_Probe() 会把两个都试一遍，结果写进 g_oled_addr。
 *
 * 屏不亮时在 CCS 里挂起程序看这两个变量:
 *   g_oled_addr  == 0  -> 总线上没有任何器件应答(供电/接线/上拉/地址)
 *   g_i2c_to_cnt  > 0  -> I2C 被拉住(没上拉、SCL/SDA 短地、从机死锁)
 */
volatile uint8_t  g_oled_addr  = 0x3CU;
volatile uint32_t g_i2c_to_cnt = 0U;

/* I2C 等待上限: 32 MHz 下大约几十毫秒, 防止总线被拉住时死等 */
#define I2C_WAIT_LOOPS   200000UL

/**
  * @brief  等 I2C 控制器进入某个状态(带超时, 不会死等)
  * @param  want_idle 1 = 等 IDLE, 0 = 等 BUSY_BUS 结束
  * @retval 1 = 等到, 0 = 超时
  */
static uint8_t OLED_I2C_Wait(uint8_t want_idle)
{
    uint32_t to = I2C_WAIT_LOOPS;

    if (want_idle != 0U)
    {
        while (((DL_I2C_getControllerStatus(I2C_OLED_INST) & DL_I2C_CONTROLLER_STATUS_IDLE) == 0U)
               && (to > 0U))
        {
            to--;
        }
    }
    else
    {
        while (((DL_I2C_getControllerStatus(I2C_OLED_INST) & DL_I2C_CONTROLLER_STATUS_BUSY_BUS) != 0U)
               && (to > 0U))
        {
            to--;
        }
    }

    if (to == 0U)
    {
        g_i2c_to_cnt++;
        return 0U;
    }
    return 1U;
}

/**
  * @brief  探测某个 7 位地址上有没有器件应答
  * @retval 1 = 有应答, 0 = 没应答(或超时)
  */
static uint8_t OLED_I2C_ProbeAddr(uint8_t addr7)
{
    uint8_t dummy = 0x00U;

    if (OLED_I2C_Wait(1U) == 0U) { return 0U; }

    DL_I2C_flushControllerTXFIFO(I2C_OLED_INST);
    DL_I2C_fillControllerTXFIFO(I2C_OLED_INST, &dummy, 1U);
    DL_I2C_startControllerTransfer(I2C_OLED_INST, addr7,
                                   DL_I2C_CONTROLLER_DIRECTION_TX, 1U);

    (void)OLED_I2C_Wait(0U);
    (void)OLED_I2C_Wait(1U);

    return ((DL_I2C_getControllerStatus(I2C_OLED_INST) & DL_I2C_CONTROLLER_STATUS_ERROR) != 0U)
           ? 0U : 1U;
}

/**
  * @brief  自动探测 OLED 地址(先 0x3C 再 0x3D)
  * @retval 探到的 7 位地址; 0 = 两个都没应答
  */
uint8_t OLED_Probe(void)
{
    if (OLED_I2C_ProbeAddr(0x3CU) != 0U) { g_oled_addr = 0x3CU; return g_oled_addr; }
    if (OLED_I2C_ProbeAddr(0x3DU) != 0U) { g_oled_addr = 0x3DU; return g_oled_addr; }
    g_oled_addr = 0U;
    return 0U;
}

/**
  * @brief  OLED写命令 (硬件I2C版)
  * @param  Command 要写入的命令
  * @retval 无
  */
void OLED_WriteCommand(uint8_t Command)
{
    uint8_t txData[2];
    txData[0] = 0x00;       // 控制字节：写命令
    txData[1] = Command;    // 命令内容

    if (g_oled_addr == 0U) { return; }
    if (OLED_I2C_Wait(1U) == 0U) { return; }

    /* 填充发射FIFO */
    DL_I2C_flushControllerTXFIFO(I2C_OLED_INST);
    DL_I2C_fillControllerTXFIFO(I2C_OLED_INST, &txData[0], 2U);

    /* 启动硬件 I2C 传输 */
    DL_I2C_startControllerTransfer(I2C_OLED_INST, g_oled_addr,
                                   DL_I2C_CONTROLLER_DIRECTION_TX, 2U);

    /* 等待传输完成 */
    (void)OLED_I2C_Wait(0U);
    (void)OLED_I2C_Wait(1U);
}

/**
  * @brief  OLED写数据 (硬件I2C版)
  * @param  Data 要写入的数据
  * @retval 无
  */
void OLED_WriteData(uint8_t Data)
{
    uint8_t txData[2];
    txData[0] = 0x40;       // 控制字节：写数据
    txData[1] = Data;       // 数据内容

    if (g_oled_addr == 0U) { return; }
    if (OLED_I2C_Wait(1U) == 0U) { return; }

    /* 填充发射FIFO */
    DL_I2C_flushControllerTXFIFO(I2C_OLED_INST);
    DL_I2C_fillControllerTXFIFO(I2C_OLED_INST, &txData[0], 2U);

    /* 启动硬件 I2C 传输 */
    DL_I2C_startControllerTransfer(I2C_OLED_INST, g_oled_addr,
                                   DL_I2C_CONTROLLER_DIRECTION_TX, 2U);

    /* 等待传输完成 */
    (void)OLED_I2C_Wait(0U);
    (void)OLED_I2C_Wait(1U);
}
/**
  * @brief  OLED设置光标位置
  * @param  Y 以左上角为原点，向下方向的坐标，范围：0~7
  * @param  X 以左上角为原点，向右方向的坐标，范围：0~127
  * @retval 无
  */
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
    OLED_WriteCommand(0xB0 | Y);                    //设置Y位置
    OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));    //设置X位置高4位
    OLED_WriteCommand(0x00 | (X & 0x0F));           //设置X位置低4位
}

/* ===========================================================================
 *  GRAM 显存 (8 页 x 128 列)
 *  所有画字符只改 RAM, 最后由 OLED_Flush() 整屏刷出去。
 *  好处: (1) 不闪屏  (2) I2C 由"每字节一次传输"变成"每页一次传输"
 * =========================================================================== */
static uint8_t g_gram[8][128];

/* 4bit -> 8bit: 每个位左右各复制一份, 用来放大字体 */
static const uint8_t g_expand4[16] = {
    0x00U, 0x03U, 0x0CU, 0x0FU, 0x30U, 0x33U, 0x3CU, 0x3FU,
    0xC0U, 0xC3U, 0xCCU, 0xCFU, 0xF0U, 0xF3U, 0xFCU, 0xFFU
};

/* FIFO 边发边补的总超时(只是安全网, 正常几毫秒就发完) */
#define I2C_FILL_TO_LOOPS   2000000UL

/**
  * @brief  一次 I2C 传输发送 n 个字节
  *         MSPM0 的 TX FIFO 只有 8 字节, 长传输要边发边补;
  *         FIFO 空了硬件会拉住 SCL 等数据, 所以这样发是安全的。
  * @retval 无
  */
uint8_t OLED_WriteBytes(const uint8_t *src, uint16_t n)
{
    uint16_t sent;
    uint32_t to;

    if ((g_oled_addr == 0U) || (n == 0U)) { return 0U; }
    if (OLED_I2C_Wait(1U) == 0U) { return 0U; }

    DL_I2C_flushControllerTXFIFO(I2C_OLED_INST);
    sent = DL_I2C_fillControllerTXFIFO(I2C_OLED_INST, (uint8_t *)src, n);

    DL_I2C_startControllerTransfer(I2C_OLED_INST, g_oled_addr,
                                   DL_I2C_CONTROLLER_DIRECTION_TX, n);

    to = I2C_FILL_TO_LOOPS;
    while ((sent < n) && (to > 0U))
    {
        /* 从机不应答就直接退出, 别在这儿耗着 */
        if ((DL_I2C_getControllerStatus(I2C_OLED_INST) & DL_I2C_CONTROLLER_STATUS_ERROR) != 0U)
        {
            break;
        }
        if (DL_I2C_isControllerTXFIFOFull(I2C_OLED_INST) == false)
        {
            sent = (uint16_t)(sent + DL_I2C_fillControllerTXFIFO(
                       I2C_OLED_INST, (uint8_t *)&src[sent], (uint16_t)(n - sent)));
        }
        to--;
    }

    if (sent < n)
    {
        g_i2c_to_cnt++;
        DL_I2C_resetControllerTransfer(I2C_OLED_INST);
        DL_I2C_flushControllerTXFIFO(I2C_OLED_INST);
        return 0U;                       /* 这一片没发完, 交给调用者重试 */
    }

    (void)OLED_I2C_Wait(0U);
    (void)OLED_I2C_Wait(1U);
    return 1U;
}

/**
  * @brief  把整块 GRAM 刷到 OLED
  *         之前是每页一次 129 字节的传输, FIFO 边发边补一旦超时/被 NAK,
  *         这一页就只写了一部分, 屏上就会出现"雪花/花屏"。
  *         现在拆成每片 32 字节, 每片独立重试, 单片失败也只影响 32 字节。
  * @retval 无
  */
void OLED_Flush(void)
{
    uint8_t  page;
    uint16_t col;
    uint16_t remaining;
    uint16_t chunk;
    uint8_t  buf[33];
    uint8_t  tries;

    buf[0] = 0x40U;                         /* 控制字节: 后面全是数据 */
    for (page = 0U; page < 8U; page++)
    {
        col = 0U;
        while (col < 128U)
        {
            remaining = (uint16_t)(128U - col);
            chunk = (remaining > 32U) ? 32U : remaining;
            memcpy(&buf[1], &g_gram[page][col], chunk);

            tries = 3U;
            while (tries > 0U)
            {
                OLED_SetCursor(page, (uint8_t)col);
                if (OLED_WriteBytes(buf, (uint16_t)(chunk + 1U)) != 0U) { break; }
                tries--;
            }
            col = (uint16_t)(col + chunk);
        }
    }
}

/**
  * @brief  OLED清屏(只清 RAM, 需要再调 OLED_Flush() 才刷到屏上)
  * @param  无
  * @retval 无
  */
void OLED_Clear(void)
{
    memset(g_gram, 0x00, sizeof(g_gram));
}

/**
  * @brief  OLED 关机: 先关显示再拉掉电源, 避免"断电瞬间雪花"
  *         直接切 P-MOS 时, 面板的 GDDRAM 里还留着上一帧内容,
  *         掉电过程中电荷泵失稳会把它显示成随机噪点(雪花)。
  *         这里依次: 关显示 -> 清显存 -> 留一点余量给命令发完,
  *         由调用者随后再切 OLED 电源轨。
  * @param  无
  * @retval 无
  */
void OLED_PowerOff(void)
{
    volatile uint32_t k;

    OLED_Clear();                       /* MCU 侧 GRAM 清 0 */
    OLED_WriteCommand(0xAE);            /* 面板立刻熄灭(最果断) */
    OLED_Flush();                       /* 再把面板 GDDRAM 也写成 0, 双保险 */

    /* ~30ms: 让上面几条 I2C 命令真正发完再切电 */
    for (k = 0U; k < 1000000U; k++)
    {
    }
}

/**
  * @brief  OLED显示一个字符 (8x16, 画进 GRAM)
  * @param  Line 行位置，范围：1~4
  * @param  Column 列位置，范围：1~16
  * @param  Char 要显示的一个字符，范围：ASCII可见字符
  * @retval 无
  */
void OLED_ShowChar(unsigned char Line, unsigned char Column, char Char)
{
    unsigned char i;
    unsigned char pg;
    unsigned char col;
    const unsigned char *glyph;

    if ((Line < 1U) || (Line > 4U) || (Column < 1U) || (Column > 16U)) { return; }
    if ((Char < ' ') || (Char > '~')) { Char = ' '; }

    glyph = &OLED_F8x16[(unsigned char)Char - (unsigned char)' '][0];
    pg    = (unsigned char)((Line - 1U) * 2U);
    col   = (unsigned char)((Column - 1U) * 8U);

    for (i = 0U; i < 8U; i++)
    {
        g_gram[pg][col + i]      = glyph[i];        //上半部分
        g_gram[pg + 1U][col + i] = glyph[i + 8U];   //下半部分
    }
}

/**
  * @brief  放大 2 倍显示字符串 (每个字符 16x32 像素, 用来显示主读数)
  * @param  Page   起始页, 范围: 0~4 (占 Page~Page+3 共 4 页 = 半个屏)
  * @param  Column 起始字符列, 范围: 0~7 (每字符 16 像素宽)
  * @param  String 字符串, 最多 8 个字符(正好铺满 128 像素)
  * @retval 无
  */
void OLED_ShowString2X(uint8_t Page, uint8_t Column, char *String)
{
    uint8_t p, k, c, ch;

    if (Page > 4U) { return; }

    for (p = 0U; p < 4U; p++)
    {
        for (k = 0U; (k < 8U) && (String[k] != '\0'); k++)
        {
            if ((Column + k) > 7U) { break; }

            ch = (uint8_t)String[k];
            if ((ch < 0x20U) || (ch > 0x7EU)) { ch = 0x20U; }

            for (c = 0U; c < 8U; c++)
            {
                /* 字模 0~7 列是上半块, 8~15 列是下半块;
                   p 的最低位决定取字节的低/高 4 位(垂直方向放大两倍) */
                uint8_t b = OLED_F8x16[ch - 0x20U][(uint8_t)(((p >> 1) * 8U) + c)];
                uint8_t v = ((p & 1U) != 0U) ? (uint8_t)(b >> 4) : (uint8_t)(b & 0x0FU);
                uint8_t e = g_expand4[v];
                uint8_t x = (uint8_t)(((Column + k) * 16U) + (c * 2U));

                g_gram[Page + p][x]      = e;
                g_gram[Page + p][x + 1U] = e;
            }
        }
    }
}

/**
  * @brief  OLED显示字符串
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  String 要显示的字符串，范围：ASCII可见字符
  * @retval 无
  */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
    uint8_t i;
    for (i = 0; String[i] != '\0'; i++)
    {
        OLED_ShowChar(Line, Column + i, String[i]);
    }
}

/**
  * @brief  OLED次方函数
  * @retval 返回值等于X的Y次方
  */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;
    while (Y--)
    {
        Result *= X;
    }
    return Result;
}

/**
  * @brief  OLED显示数字（十进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~4294967295
  * @param  Length 要显示数字的长度，范围：1~10
  * @retval 无
  */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)                            
    {
        OLED_ShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
    }
}

/**
  * @brief  OLED显示数字（十进制，带符号数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：-2147483648~2147483647
  * @param  Length 要显示数字的长度，范围：1~10
  * @retval 无
  */
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
    uint8_t i;
    uint32_t Number1;
    if (Number >= 0)
    {
        OLED_ShowChar(Line, Column, '+');
        Number1 = Number;
    }
    else
    {
        OLED_ShowChar(Line, Column, '-');
        Number1 = -Number;
    }
    for (i = 0; i < Length; i++)                            
    {
        OLED_ShowChar(Line, Column + i + 1, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0');
    }
}

/**
  * @brief  OLED显示数字（十六进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~0xFFFFFFFF
  * @param  Length 要显示数字的长度，范围：1~8
  * @retval 无
  */
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i, SingleNumber;
    for (i = 0; i < Length; i++)                            
    {
        SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
        if (SingleNumber < 10)
        {
            OLED_ShowChar(Line, Column + i, SingleNumber + '0');
        }
        else
        {
            OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
        }
    }
}

/**
  * @brief  OLED显示数字（二进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~1111 1111 1111 1111
  * @param  Length 要显示数字的长度，范围：1~16
  * @retval 无
  */
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)                            
    {
        OLED_ShowChar(Line, Column + i, Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
    }
}

/**
  * @brief  OLED初始化
  * @param  无
  * @retval 无
  */
void OLED_Init(void)
{
    uint32_t i, j;
    uint8_t  page;

    /* 上电延时: SSD1306 内部 POR + 电荷泵建立需要时间。
     * 太短会把初始化命令丢掉, 而屏上电时 GDDRAM 是随机值 ->
     * 表现就是"背光亮、满屏雪花"。这里加到约 200ms。 */
    for (i = 0; i < 6400; i++)
    {
        for (j = 0; j < 1000; j++);
    }

    /* 自动认地址 0x3C / 0x3D; 认不到就把 g_oled_addr 置 0, 后面所有写操作直接返回 */
    (void)OLED_Probe();

    OLED_WriteCommand(0xAE);    //关闭显示

    /* 趁显示还没开, 把面板内部 GDDRAM 整块写 0x00。
     * 上电时 GDDRAM 是随机值, 一旦 0xAF 开显示而 RAM 没清干净,
     * 就会看到"整屏雪花"。这一步是幂等的, 屏正常时也无害。 */
    for (page = 0U; page < 8U; page++)
    {
        OLED_SetCursor(page, 0U);
        for (i = 0U; i < 128U; i++)
        {
            OLED_WriteData(0x00U);
        }
    }

    OLED_WriteCommand(0xD5);    //设置显示时钟分频比/振荡器频率
    OLED_WriteCommand(0x80);
    
    OLED_WriteCommand(0xA8);    //设置多路复用率
    OLED_WriteCommand(0x3F);
    
    OLED_WriteCommand(0xD3);    //设置显示偏移
    OLED_WriteCommand(0x00);
    
    OLED_WriteCommand(0x40);    //设置显示开始行
    
    OLED_WriteCommand(0xA1);    //设置左右方向，0xA1正常 0xA0左右反置
    
    OLED_WriteCommand(0xC8);    //设置上下方向，0xC8正常 0xC0上下反置

    OLED_WriteCommand(0xDA);    //设置COM引脚硬件配置
    OLED_WriteCommand(0x12);
    
    OLED_WriteCommand(0x81);    //设置对比度控制
    /* 对比度直接决定 OLED 电流和亮度。原来用 0xCF(最大附近), 电流大 ->
     * 8800µF 上的电压被拉低, 放电过程中屏会越来越暗(先亮后暗)。
     * 降到 0x8F 让电流小一些、全程亮度更均匀。
     * 想再亮一点改 0xA8 / 0xBF(费电), 想更省电改 0x6F。 */
    OLED_WriteCommand(0x8F);

    OLED_WriteCommand(0xD9);    //设置预充电周期
    OLED_WriteCommand(0xF1);

    OLED_WriteCommand(0xDB);    //设置VCOMH取消选择级别
    OLED_WriteCommand(0x30);

    OLED_WriteCommand(0xA4);    //设置整个显示打开/关闭

    OLED_WriteCommand(0xA6);    //设置正常/倒转显示

    OLED_WriteCommand(0x8D);    //设置充电泵
    OLED_WriteCommand(0x14);

    OLED_WriteCommand(0xAF);    //开启显示
        
    OLED_Clear();                //清 RAM
    OLED_Flush();                //刷到屏上
}