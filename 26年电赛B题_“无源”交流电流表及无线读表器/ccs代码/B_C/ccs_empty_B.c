/*
 * ===========================================================================
 *  ccs_empty_B.c     2026 全国大学生电子设计竞赛 B 题
 * ===========================================================================
 */

#include "ti/driverlib/dl_adc12.h"
#include "ti/driverlib/dl_timera.h"
#include "ti/driverlib/dl_uart_main.h"
#include "ti_msp_dl_config.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "OLED.h"

/* ==================== 继电器双 IO 控制 ==================== */
#define RELAY_ACTIVE_LOW        1

#define BT_PWR_ON()       DL_GPIO_clearPins(BT_PWR_PORT, BT_PWR_PIN_6_PIN)
#define BT_PWR_OFF()      DL_GPIO_setPins  (BT_PWR_PORT, BT_PWR_PIN_6_PIN)

#if RELAY_ACTIVE_LOW
#define RELAY_CHG_ON()    DL_GPIO_setPins  (RELAY_PA07_PORT, RELAY_PA07_PIN_7_PIN)
#define RELAY_CHG_OFF()   DL_GPIO_clearPins(RELAY_PA07_PORT, RELAY_PA07_PIN_7_PIN)
#define RELAY_MEAS_ON()   DL_GPIO_clearPins(RELAY_PA09_PORT, RELAY_PA09_PIN_9_PIN)
#define RELAY_MEAS_OFF()  DL_GPIO_setPins  (RELAY_PA09_PORT, RELAY_PA09_PIN_9_PIN)
#else
#define RELAY_CHG_ON()    DL_GPIO_clearPins(RELAY_PA07_PORT, RELAY_PA07_PIN_7_PIN)
#define RELAY_CHG_OFF()   DL_GPIO_setPins  (RELAY_PA07_PORT, RELAY_PA07_PIN_7_PIN)
#define RELAY_MEAS_ON()   DL_GPIO_setPins  (RELAY_PA09_PORT, RELAY_PA09_PIN_9_PIN)
#define RELAY_MEAS_OFF()  DL_GPIO_clearPins(RELAY_PA09_PORT, RELAY_PA09_PIN_9_PIN)
#endif

#define RELAY_DEADTIME_CYCLES   96000U

static void Relay_AllOff(void)
{
    RELAY_MEAS_OFF();
    RELAY_CHG_OFF();
    delay_cycles(RELAY_DEADTIME_CYCLES);
}

static void Relay_Charge(void)
{
    RELAY_MEAS_OFF();
    delay_cycles(RELAY_DEADTIME_CYCLES);
    RELAY_CHG_ON();
}

static void Relay_Measure(void)
{
    RELAY_CHG_OFF();
    delay_cycles(RELAY_DEADTIME_CYCLES);
    RELAY_MEAS_ON();
    delay_cycles(RELAY_DEADTIME_CYCLES);
}

/* ==================== 采样窗口 ==================== */
#define PERIODS_TO_SAMPLE        7U
#define SAMPLE_POINTS_PER_PERIOD 512U
#define TOTAL_SAMPLES            (PERIODS_TO_SAMPLE * SAMPLE_POINTS_PER_PERIOD)

/* ==================== 剔除规则 ==================== */
#define TRIM_RULE                3U

/* ==================== 前端常数 ==================== */
#define VREF_V          3.3f
#define ADC_FULL        4096.0f
#define SHUNT_RESISTOR  20.0f
#define CT_TURNS_PRI    5.0f
#define CT_TURNS_SEC    200.0f

#define CURRENT_SCALE   ((CT_TURNS_SEC / CT_TURNS_PRI) / SHUNT_RESISTOR)

/* 本版【去掉补偿】, 直接按理想变比反算一次侧电流:
 *     g_i_rms = v_rms * CURRENT_SCALE;   (CURRENT_SCALE = (200/5)/20 = 2.0 A/V)
 * 原分段一点标定(cal_apply() 与 CAL_RAW_LO / CAL_GAIN / CAL_OFFSET_A 系数表)已整体删除,
 * 读数就是前端原始反算值, 不再乘增益、不再修正零漂。
 * 以后若要恢复标定, 从 B6 的 ccs_empty_B.c 里把那段系数表和 cal_apply() 拷回来,
 * 再把 measure_primary_rms() 末尾那一行换回 cal_apply() 即可。 */

#define ADC_WAIT_LOOPS  200000UL

/* ==================== OLED I2C 速率（覆盖 SysConfig 生成值） ====================
 * SysConfig（Fast/400kHz）生成的 ti_msp_dl_config.c 里是
 *     DL_I2C_setTimerPeriod(I2C_OLED_INST, 7);
 * 这个值比 SSD1306 的 400kHz 上限更高（见下面换算），属于“有波形但不应答
 * → 一直黑屏”的典型情形；长走线把边沿拖圆后更容易出问题。
 * 这里在 SYSCFG_DL_init() 之后重新写一次 MTPR，先降到约 100kHz 求稳。
 *
 * MTPR 换算：I2C 模块对 BUSCLK 有内部分频（÷8），所以
 *     f_SCL = (32MHz / 8) / (MTPR + 1) = 4MHz / (MTPR + 1)
 * SysConfig 给 Fast 档写的是 MTPR = 7 → 约 500kHz；
 * 要 400kHz 用 MTPR = 9，要 100kHz 用 MTPR = 39，要 50kHz 用 MTPR = 79。
 * 手上如果有示波器，量一次 SCL 周期确认；改回 7 就等于还原 SysConfig 的原值。
 * ============================================================================ */
#define I2C_OLED_TPR_100K   39U

/* 想调速率只改上面那一个数；改成 7 就等于还原成 SysConfig 的 400kHz */
static void OLED_I2C_SetSpeed(uint32_t tpr)
{
    DL_I2C_setTimerPeriod(I2C_OLED_INST, tpr);
}

/* 采样稳定策略 */
#define MEASURE_RETRY_COUNT       8U
#define MEASURE_RETRY_GAP_CYCLES  6400000U
#define MEASURE_MAX_VALID_A       2.5f

#define ADC_CLIP_LO     2U
#define ADC_CLIP_HI    4093U

#define POWER_SETTLE_CYCLES     6400000U

#define ALWAYS_ON_MODE          0U
#define MEASURE_INTERVAL_CYCLES 160000000U

/* 调试显示开关：0 = 只显示地址和电流 */
#define DEBUG_SHOW_RAW          0U

#define BT_USE_DETECT_PIN       0U
#define BT_REPEAT_COUNT         0U
#define BT_GAP_CYCLES           1600000U

/* ==================== 电流区间阈值 ==================== */
#define SMALL_CURRENT_THRESHOLD   0.30f

/* ==================== 小电流分支参数（<0.30A） ==================== */
#define SMALL_OLED_DISPLAY_CYCLES   32000000U   /* 32MHz -> 约 1 秒 */
#define SMALL_BURST_COUNT           4U           /* 蓝牙分 4 轮广播 */
#define SMALL_BURST_FRAMES          6U           /* 每轮发 6 帧，约 3 秒 */
#define SMALL_BURST_GAP_CYCLES      32000000U    /* 轮间隔约 1 秒，让 VCAP 恢复 */

/* ==================== 大电流分支参数（>=0.30A） ==================== */
#define LARGE_BURST_COUNT           1U           /* 蓝牙广播 1 轮 */
#define LARGE_BURST_FRAMES          5U           /* 每轮发 5 帧，约 2.5 秒 */

/* ==================== 采样缓冲 ==================== */
static uint16_t adc_buffer[TOTAL_SAMPLES];

/* ==================== 调试变量 ==================== */
volatile float    g_i_rms  = 0.0f;
volatile float    g_v_rms  = 0.0f;
volatile float    g_v_dc   = 0.0f;
volatile float    g_cyc_v[PERIODS_TO_SAMPLE];
volatile float    g_cyc_dc[PERIODS_TO_SAMPLE];
volatile uint8_t  g_n_keep = 0U;
volatile float    g_drop_max = 0.0f;
volatile float    g_drop_min = 0.0f;
volatile uint8_t  g_drop_max_idx = 0U;
volatile uint8_t  g_drop_min_idx = 0U;
volatile uint16_t g_c_min  = 0U;
volatile uint16_t g_c_max  = 0U;
volatile uint32_t g_adc_to = 0U;
volatile uint8_t  g_adc_ok = 1U;
volatile uint8_t  g_clip   = 0U;

/* ==================== 全局变量 ==================== */
bool     bluetooth_present = false;

/* ==================== 函数声明 ==================== */
void uart0_send_packet(uint8_t addr, float amps, uint8_t status);
static uint8_t bt_status_of(float amps);
float measure_primary_rms(void);
static float measure_filtered_current(void);
void uart0_send_string(char* str);
uint8_t Get_Device_Address(void);
static void OLED_ShowAddrCurrent(uint8_t addr, float current);

/* ==================== 切到测量前的静置 ====================
 * 先断开充电支路, 等 POWER_SETTLE_CYCLES(200ms), 再由 Relay_Measure() 合上
 * 测量支路(它内部还有 3ms 先断后合死区), 合起来就是 "200ms + 3ms"。
 *
 * 上电第一次测量本来就走的这条路(见 main 里单次模式开头); 常在线时每次
 * 重测原来只有 3ms —— AQY412(常闭)可能还没完全断开、二次侧也没进入新稳态,
 * 整流桥和 20Ω 会短暂并联。按 接线说明.md 第 4 节, 那会让读数整体偏小。
 * 这里把两条路径统一成同一条。 */
static void Relay_SettleBeforeMeasure(void)
{
    RELAY_CHG_OFF();
    delay_cycles(POWER_SETTLE_CYCLES);
}

int main(void)
{
    float final_current = 0.0f;
    uint8_t dev_addr = 0;
    uint16_t bt_i = 0U;

    SYSCFG_DL_init();

    /* 把 SysConfig 生成的 400kHz 降到约 100kHz，屏幕调通后再改回 7 */
    OLED_I2C_SetSpeed(I2C_OLED_TPR_100K);

    BT_PWR_OFF();

    DL_GPIO_setPins(OLED_PWR_PORT, OLED_PWR_PIN_8_PIN);

    Relay_Charge();

#if BT_USE_DETECT_PIN
    if (DL_GPIO_readPins(BT_DETECT_PORT, BT_DETECT_PIN_4_PIN) == 0) {
        bluetooth_present = true;
    } else {
        bluetooth_present = false;
        DL_UART_Main_disablePower(UART_0_INST);
    }
#else
    bluetooth_present = true;
#endif

#if ALWAYS_ON_MODE
    delay_cycles(POWER_SETTLE_CYCLES);

    DL_GPIO_clearPins(OLED_PWR_PORT, OLED_PWR_PIN_8_PIN);
    delay_cycles(640000);
    OLED_Init();

    BT_PWR_ON();
    delay_cycles(16000000);

    if (bluetooth_present) {
        uart0_send_string("=== Powered On, Always-On Mode ===\r\n");
    }

    while (1)
    {
        delay_cycles(MEASURE_INTERVAL_CYCLES);

        /* 与上电第一次同一条路径: 断开充电后静置 200ms + 3ms 死区 */
        Relay_SettleBeforeMeasure();
        Relay_Measure();
        final_current = measure_primary_rms();
        Relay_Charge();

        dev_addr = Get_Device_Address();
        OLED_ShowAddrCurrent(dev_addr, final_current);

        uart0_send_packet(dev_addr, final_current, bt_status_of(final_current));
    }
#else
    /* ==========================================================
     * 单次模式: 全量程统一成一个循环
     *   每次测量前都走同一条静置路径(断充电 -> 等 200ms -> 再切测量),
     *   不因电流大小、也不因第几次测量而不同。
     *   小电流(<0.30A): 测一次 -> OLED 亮约 1 秒 -> 熄屏 ->
     *                   蓝牙 4 轮 x 6 帧 -> 回到测量再来一轮
     *   大电流(>=0.30A): OLED/蓝牙保持在线, 每轮 1 x 5 帧 ->
     *                   重测一次 -> 刷新显示 -> 再发一轮
     * ========================================================== */
    while (1)
    {
        /* ---------------- 1. 统一静置后采一次 ---------------- */
        /* 先断充电, 静置 200ms, 再切测量(Relay_Measure 内部还有 3ms 死区) */
        Relay_SettleBeforeMeasure();

        Relay_Measure();
        final_current = measure_filtered_current();
        Relay_Charge();

        /* ---------------- 2. 送显 ---------------- */
        DL_GPIO_clearPins(OLED_PWR_PORT, OLED_PWR_PIN_8_PIN);
        delay_cycles(6400000);              /* ~200ms: 等 OLED 内部 POR 完成, 避免初始化被吃掉 */
        OLED_Init();

        dev_addr = Get_Device_Address();
        OLED_ShowAddrCurrent(dev_addr, final_current);

        /* ---------------- 3. 小电流(<0.30A): 4 轮 x 6 帧 ---------------- */
        if (final_current < SMALL_CURRENT_THRESHOLD)
        {
            /* 0A ~ 0.30A：小电流模式。
             * OLED 只亮 1 秒，蓝牙分 4 轮短时广播，每轮 6 帧(约 3 秒),
             * 轮间约 1 秒让 VCAP 恢复; 报完回到本循环顶部再测一次。 */
            delay_cycles(SMALL_OLED_DISPLAY_CYCLES);
            OLED_PowerOff();                                /* 先熄屏清显存, 再切电源 */
            DL_GPIO_setPins(OLED_PWR_PORT, OLED_PWR_PIN_8_PIN);

            for (uint8_t burst = 0U; burst < SMALL_BURST_COUNT; burst++)
            {
                BT_PWR_ON();
                delay_cycles(16000000);   /* 0.5s 等待蓝牙启动 */

                for (bt_i = 0U; bt_i < SMALL_BURST_FRAMES; bt_i++)
                {
                    uart0_send_packet(dev_addr, final_current,
                                      bt_status_of(final_current));
                    delay_cycles(16000000);   /* 0.5s，6 帧共约 3 秒 */
                }

                BT_PWR_OFF();
                delay_cycles(SMALL_BURST_GAP_CYCLES);   /* 约 1 秒，让 VCAP 恢复 */
            }

            /* 回到本循环顶部: 重新静置 -> 测量 -> 显示 -> 再发 4 轮 x 6 帧 */
            continue;
        }
        /* ---------------- 4. 大电流(>=0.30A): 1 轮 x 5 帧 ----------------
         * OLED 与蓝牙保持工作; 每轮只发 5 帧(约 2.5 秒), 发完重测一次,
         * 刷新显示后再发下一轮。 */
        BT_PWR_ON();
        delay_cycles(16000000);   /* 0.5s 等待蓝牙启动 */

        while (1)
        {
            for (uint8_t burst = 0U; burst < LARGE_BURST_COUNT; burst++)
            {
                for (bt_i = 0U; bt_i < LARGE_BURST_FRAMES; bt_i++)
                {
                    uart0_send_packet(dev_addr, final_current,
                                      bt_status_of(final_current));
                    delay_cycles(16000000);   /* 0.5s, 5 帧共约 2.5 秒 */
                }
            }

            /* 重新采集一次: 同样先断充电静置 200ms + 3ms 死区, 与上电第一次一致 */
            Relay_SettleBeforeMeasure();
            Relay_Measure();
            final_current = measure_filtered_current();
            Relay_Charge();

            /* 刷新 OLED */
            dev_addr = Get_Device_Address();
            OLED_ShowAddrCurrent(dev_addr, final_current);

            /* 电流掉回小电流区间: 熄屏后复位, 从统一循环顶部重新走 */
            if (final_current < SMALL_CURRENT_THRESHOLD)
            {
                BT_PWR_OFF();
                OLED_PowerOff();                        /* 复位前先熄屏清显存 */
                DL_GPIO_setPins(OLED_PWR_PORT, OLED_PWR_PIN_8_PIN);
                NVIC_SystemReset();
            }
        }
    }
#endif
}

/* --------------------------------------------------------------------------- */
static void OLED_ShowAddrCurrent(uint8_t addr, float current)
{
    char line[24];

    sprintf(line, "ADDR%02u", (unsigned)addr);
    OLED_ShowString(1U, 1U, line);

    sprintf(line, "%.4f A", (double)current);
    OLED_ShowString2X(3U, 0U, line);

    OLED_Flush();
}

/* --------------------------------------------------------------------------- */
static uint8_t adc_read_one(uint16_t *code)
{
    uint32_t to = ADC_WAIT_LOOPS;

    DL_ADC12_enableConversions(ADC_VOLTAGE_INST);

    while (((DL_ADC12_getRawInterruptStatus(ADC_VOLTAGE_INST,
                 DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED)) == 0U) && (to > 0U))
    {
        to--;
    }

    if (to == 0U)
    {
        g_adc_to++;
        return 0U;
    }

    DL_ADC12_clearInterruptStatus(ADC_VOLTAGE_INST,
               DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);

    *code = (uint16_t)DL_ADC12_getMemResult(ADC_VOLTAGE_INST, DL_ADC12_MEM_IDX_0);
    return 1U;
}

/* --------------------------------------------------------------------------- */
static float srt_median(float *a, uint8_t n)
{
    uint8_t i, j;
    float   t;

    for (i = 1U; i < n; i++)
    {
        t = a[i];
        j = i;
        while ((j > 0U) && (a[j - 1U] > t))
        {
            a[j] = a[j - 1U];
            j--;
        }
        a[j] = t;
    }
    return a[n / 2U];
}

/* --------------------------------------------------------------------------- */
static void pick_periods(uint8_t *keep)
{
    uint8_t p;

    for (p = 0U; p < PERIODS_TO_SAMPLE; p++) { keep[p] = 1U; }

#if (TRIM_RULE == 1U)
    {
        uint8_t imax = 0U, imin;

        for (p = 1U; p < PERIODS_TO_SAMPLE; p++)
        {
            if (g_cyc_v[p] > g_cyc_v[imax]) { imax = p; }
        }
        keep[imax] = 0U;

        imin = (imax == 0U) ? 1U : 0U;
        for (p = 0U; p < PERIODS_TO_SAMPLE; p++)
        {
            if (keep[p] == 0U) { continue; }
            if (g_cyc_v[p] < g_cyc_v[imin]) { imin = p; }
        }
        keep[imin] = 0U;
    }

#elif (TRIM_RULE == 2U)
    {
        float tmp[PERIODS_TO_SAMPLE];
        float med, d0, d1;
        uint8_t i0 = 0U, i1 = 0U;

        for (p = 0U; p < PERIODS_TO_SAMPLE; p++) { tmp[p] = g_cyc_v[p]; }
        med = srt_median(tmp, PERIODS_TO_SAMPLE);

        d0 = -1.0f; d1 = -1.0f;
        for (p = 0U; p < PERIODS_TO_SAMPLE; p++)
        {
            float d = fabsf(g_cyc_v[p] - med);
            if (d > d0) { d1 = d0; i1 = i0; d0 = d; i0 = p; }
            else if (d > d1) { d1 = d; i1 = p; }
        }
        keep[i0] = 0U;
        keep[i1] = 0U;
    }

#else
    {
        float tmp[PERIODS_TO_SAMPLE];
        float dev[PERIODS_TO_SAMPLE];
        float med, mad;
        uint8_t nkeep = 0U, k;

        for (p = 0U; p < PERIODS_TO_SAMPLE; p++) { tmp[p] = g_cyc_v[p]; }
        med = srt_median(tmp, PERIODS_TO_SAMPLE);

        for (p = 0U; p < PERIODS_TO_SAMPLE; p++)
        {
            dev[p] = fabsf(g_cyc_v[p] - med);
            tmp[p] = dev[p];
        }
        mad = 1.4826f * srt_median(tmp, PERIODS_TO_SAMPLE);
        if (mad < 1.0e-7f) { mad = 1.0e-7f; }

        for (p = 0U; p < PERIODS_TO_SAMPLE; p++)
        {
            keep[p] = (dev[p] <= (3.0f * mad)) ? 1U : 0U;
            if (keep[p] != 0U) { nkeep++; }
        }

        if (nkeep < 4U)
        {
            for (p = 0U; p < PERIODS_TO_SAMPLE; p++) { keep[p] = 0U; }
            for (k = 0U; k < 4U; k++)
            {
                uint8_t bi = 0U;
                float   bv = 1.0e30f;

                for (p = 0U; p < PERIODS_TO_SAMPLE; p++)
                {
                    if (keep[p] != 0U) { continue; }
                    if (dev[p] < bv) { bv = dev[p]; bi = p; }
                }
                keep[bi] = 1U;
            }
        }
    }
#endif
}

/* --------------------------------------------------------------------------- */
float measure_primary_rms(void)
{
    uint32_t i, base, sum;
    uint8_t  p, imax, kept;
    uint8_t  keep[PERIODS_TO_SAMPLE];
    uint8_t  cycle_clip[PERIODS_TO_SAMPLE];
    uint8_t  clip_count[PERIODS_TO_SAMPLE];
    float    dc, sq, acc, v_rms;
    uint16_t c  = 0U;
    uint16_t lo = 0xFFFFU, hi = 0U;

    g_adc_ok = 1U;
    g_c_min  = 0U;
    g_c_max  = 0U;
    g_clip   = 0U;

    for (p = 0U; p < PERIODS_TO_SAMPLE; p++)
    {
        cycle_clip[p] = 0U;
        clip_count[p] = 0U;
    }

    DL_ADC12_clearInterruptStatus(ADC_VOLTAGE_INST,
               DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);

    DL_TimerA_startCounter(TIMER_ADC_INST_INST);

    for (i = 0U; i < TOTAL_SAMPLES; i++)
    {
        if (adc_read_one(&c) == 0U)
        {
            g_adc_ok = 0U;
            break;
        }
        adc_buffer[i] = c;
        if (c < lo) { lo = c; }
        if (c > hi) { hi = c; }

        p = (uint8_t)(i / SAMPLE_POINTS_PER_PERIOD);
        if ((c <= ADC_CLIP_LO) || (c >= ADC_CLIP_HI))
        {
            clip_count[p]++;
            if (clip_count[p] >= 2U)
            {
                cycle_clip[p] = 1U;
            }
        }
    }

    DL_TimerA_stopCounter(TIMER_ADC_INST_INST);
    DL_ADC12_disableConversions(ADC_VOLTAGE_INST);

    g_c_min = lo;
    g_c_max = hi;

    if (g_adc_ok == 0U)
    {
        g_i_rms = 0.0f;
        g_v_rms = 0.0f;
        g_v_dc  = 0.0f;
        return 0.0f;
    }

    for (p = 0U; p < PERIODS_TO_SAMPLE; p++)
    {
        base = (uint32_t)p * SAMPLE_POINTS_PER_PERIOD;

        sum = 0U;
        for (i = base; i < (base + SAMPLE_POINTS_PER_PERIOD); i++)
        {
            sum += (uint32_t)adc_buffer[i];
        }
        dc = (float)sum / (float)SAMPLE_POINTS_PER_PERIOD;

        sq = 0.0f;
        for (i = base; i < (base + SAMPLE_POINTS_PER_PERIOD); i++)
        {
            float d = (float)adc_buffer[i] - dc;
            sq += d * d;
        }
        sq /= (float)SAMPLE_POINTS_PER_PERIOD;

        g_cyc_dc[p] = dc * (VREF_V / ADC_FULL);
        g_cyc_v[p]  = sqrtf(sq) * (VREF_V / ADC_FULL);
    }

    pick_periods(keep);

    for (p = 0U; p < PERIODS_TO_SAMPLE; p++)
    {
        if (cycle_clip[p] != 0U)
        {
            keep[p] = 0U;
        }
    }

    acc   = 0.0f;
    kept  = 0U;
    imax  = 0U;
    g_drop_max = 0.0f;
    g_drop_max_idx = 0U;
    g_drop_min = 0.0f;
    g_drop_min_idx = 0U;

    for (p = 0U; p < PERIODS_TO_SAMPLE; p++)
    {
        if (keep[p] != 0U)
        {
            acc += g_cyc_v[p];
            kept++;
            continue;
        }

        imax++;
        if ((g_drop_max == 0.0f) || (g_cyc_v[p] > g_drop_max))
        {
            g_drop_max = g_cyc_v[p];
            g_drop_max_idx = p;
        }
        if ((g_drop_min == 0.0f) || (g_cyc_v[p] < g_drop_min))
        {
            g_drop_min = g_cyc_v[p];
            g_drop_min_idx = p;
        }
    }

    v_rms    = (kept > 0U) ? (acc / (float)kept) : 0.0f;
    g_n_keep = kept;

    if (kept < 4U)
    {
        g_clip = 1U;
        g_i_rms = 0.0f;
        g_v_rms = 0.0f;
        g_v_dc  = 0.0f;
        return 0.0f;
    }

    acc  = 0.0f;
    kept = 0U;
    for (p = 0U; p < PERIODS_TO_SAMPLE; p++)
    {
        if (keep[p] == 0U) { continue; }
        acc += g_cyc_dc[p];
        kept++;
    }
    g_v_dc = (kept > 0U) ? (acc / (float)kept) : 0.0f;

    g_v_rms = v_rms;
    /* 不做补偿: 直接按理想变比反算一次侧电流有效值 */
    g_i_rms = v_rms * CURRENT_SCALE;

    return g_i_rms;
}

/* --------------------------------------------------------------------------- */
static float measure_filtered_current(void)
{
    uint8_t retry;
    float current = 0.0f;
    static float last_valid_current = 0.0f;

    for (retry = 0U; retry < MEASURE_RETRY_COUNT; retry++)
    {
        current = measure_primary_rms();

        if ((g_adc_ok != 0U) &&
            (g_clip == 0U) &&
            (current <= MEASURE_MAX_VALID_A))
        {
            last_valid_current = current;
            return current;
        }

        delay_cycles(MEASURE_RETRY_GAP_CYCLES);
    }

    g_clip = 0U;
    g_adc_ok = 1U;

    return last_valid_current;
}

/* --------------------------------------------------------------------------- */
uint8_t Get_Device_Address(void)
{
    uint8_t address = 0;
    /* 拨码开关跨 PA/PB 两个端口, 所以按位分别指定端口:
     *   bit0 = PB26, bit1 = PA25, bit2 = PA23, bit3 = PB23
     * 低电平有效(开关拨到 GND 时该位为 1)。 */
    if (DL_GPIO_readPins(ADDR_SW_B_PORT, ADDR_SW_B_PIN_B26_PIN) == 0) address |= (1 << 0);
    if (DL_GPIO_readPins(ADDR_SW_A_PORT, ADDR_SW_A_PIN_A25_PIN) == 0) address |= (1 << 1);
    if (DL_GPIO_readPins(ADDR_SW_A_PORT, ADDR_SW_A_PIN_A23_PIN) == 0) address |= (1 << 2);
    if (DL_GPIO_readPins(ADDR_SW_B_PORT, ADDR_SW_B_PIN_B23_PIN) == 0) address |= (1 << 3);
    return address;
}

/* --------------------------------------------------------------------------- */
void uart0_send_string(char* str)
{
    uint32_t timeout;

    if (!bluetooth_present)
    {
        return;
    }

    while (*str != '\0')
    {
        timeout = 200000UL;

        while ((DL_UART_Main_isTXFIFOFull(UART_0_INST) == true) &&
               (timeout > 0U))
        {
            timeout--;
        }

        if (timeout == 0U)
        {
            return;
        }

        DL_UART_Main_transmitData(UART_0_INST, (uint8_t)(*str));
        str++;
    }

    timeout = 200000UL;
    while ((DL_UART_Main_isTXFIFOEmpty(UART_0_INST) == false) &&
           (timeout > 0U))
    {
        timeout--;
    }
}

/* --------------------------------------------------------------------------- */
static uint8_t bt_status_of(float amps)
{
    if (g_adc_ok == 0U) { return 3U; }
    if (g_clip   != 0U) { return 3U; }
    if (amps < 0.2f)    { return 1U; }
    if (amps > 2.0f)    { return 2U; }
    return 0U;
}

/* --------------------------------------------------------------------------- */
void uart0_send_packet(uint8_t addr, float amps, uint8_t status)
{
    char frame[64];

    if ((g_adc_ok == 0U) || (g_clip != 0U))
    {
        return;
    }

    (void)status;

    sprintf(frame, "I%02u,%.4fA\n",
            (unsigned)addr, (double)amps);

    uart0_send_string(frame);
}
