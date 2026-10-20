## 引脚变更记录（本次修改：PA6/PA8/PA10/PA11 与拨码开关全部改位）

改的是 `ccs_empty_B.syscfg`，并已用 SysConfig CLI（`sysconfig_1.28.0`，device `MSPM0G3507`，package `LQFP-64(PM)`）重新生成 `Debug/ti_msp_dl_config.c/.h`。

| 功能 | 原来 | 现在 | 备注 |
| --- | --- | --- | --- |
| 蓝牙电源 P-MOS 栅极 | PA6 | **PA30** | 纯 GPIO 推挽输出，初值高（关断） |
| OLED 电源 P-MOS 栅极 | PA8 | **PA16** | 纯 GPIO 推挽输出，初值高（关断） |
| 蓝牙 RXD（M0 的 UART0 TX） | PA10 | **PA28** | UART0 合法位 |
| 蓝牙 TXD（M0 的 UART0 RX） | PA11 | **PA31** | UART0 合法位，见下方"为什么不是 PA29" |
| 拨码 bit0 | PB8 | **PB26** | 输入 + 内部上拉 |
| 拨码 bit1 | PB12 | **PA25** | 输入 + 内部上拉 |
| 拨码 bit2 | PB23 | **PA23** | 输入 + 内部上拉 |
| 拨码 bit3 | PB27 | **PB23** | 输入 + 内部上拉 |

### 为什么蓝牙 RX 是 PA31 而不是 PA29

UART0 的 TX/RX 必须落在**同一组 pad** 上（MSPM0 的 PINCM 编号每 10 个一组，个位相同即同组）。这颗芯片在 LQFP-64(PM) 上实际可用的 UART0 引脚是：

| TX | RX |
| --- | --- |
| PA0 | PA1 |
| PA10 | PA11 |
| PA28 | **PA31** |

**PA29 根本没有 UART0 功能**——查器件数据（`deviceData/MSPM0G350X/MSPM0G350X.json` 的 mux 表），PA29 只有 `I2C1.SCL / UART2.RTS / TIMG8.CCP0 / TIMG6.CCP0`。PA0/PA1 被 OLED 的 I2C0 占用，所以除 PA10/PA11 之外唯一能用的就是 **PA28 + PA31**。SysConfig 对 PA28/PA29 组合会直接报错。

### 两个 GPIO 实例

拨码开关跨了 PA 和 PB 两个端口，而一个 GPIO 实例只能属于一个端口，所以在 syscfg 里拆成两组：

- `ADDR_SW_B`（PORTB）：`PIN_B26` = PB26、`PIN_B23` = PB23
- `ADDR_SW_A`（PORTA）：`PIN_A25` = PA25、`PIN_A23` = PA23

固件里 `Get_Device_Address()` 已按位分别指定端口，位序不变（bit0=PB26、bit1=PA25、bit2=PA23、bit3=PB23）。

### 方向速记

- M0 的 **TX (PA28)** → 蓝牙模块的 **RXD**
- 蓝牙模块的 **TXD** → M0 的 **RX (PA31)**

---

## Example Summary

Empty project using DriverLib.
This example shows a basic empty project using DriverLib with just main file
and SysConfig initialization.

## Peripherals & Pin Assignments

| Peripheral | Pin | Function |
| --- | --- | --- |
| SYSCTL |  |  |
| DEBUGSS | PA20 | Debug Clock |
| DEBUGSS | PA19 | Debug Data In Out |

## BoosterPacks, Board Resources & Jumper Settings

Visit [LP_MSPM0G3507](https://www.ti.com/tool/LP-MSPM0G3507) for LaunchPad information, including user guide and hardware files.

| Pin | Peripheral | Function | LaunchPad Pin | LaunchPad Settings |
| --- | --- | --- | --- | --- |
| PA20 | DEBUGSS | SWCLK | N/A | <ul><li>PA20 is used by SWD during debugging<br><ul><li>`J101 15:16 ON` Connect to XDS-110 SWCLK while debugging<br><li>`J101 15:16 OFF` Disconnect from XDS-110 SWCLK if using pin in application</ul></ul> |
| PA19 | DEBUGSS | SWDIO | N/A | <ul><li>PA19 is used by SWD during debugging<br><ul><li>`J101 13:14 ON` Connect to XDS-110 SWDIO while debugging<br><li>`J101 13:14 OFF` Disconnect from XDS-110 SWDIO if using pin in application</ul></ul> |

### Device Migration Recommendations
This project was developed for a superset device included in the LP_MSPM0G3507 LaunchPad. Please
visit the [CCS User's Guide](https://software-dl.ti.com/msp430/esd/MSPM0-SDK/latest/docs/english/tools/ccs_ide_guide/doc_guide/doc_guide-srcs/ccs_ide_guide.html#sysconfig-project-migration)
for information about migrating to other MSPM0 devices.

### Low-Power Recommendations
TI recommends to terminate unused pins by setting the corresponding functions to
GPIO and configure the pins to output low or input with internal
pullup/pulldown resistor.

SysConfig allows developers to easily configure unused pins by selecting **Board**→**Configure Unused Pins**.

For more information about jumper configuration to achieve low-power using the
MSPM0 LaunchPad, please visit the [LP-MSPM0G3507 User's Guide](https://www.ti.com/lit/slau873).

## Example Usage

Compile, load and run the example.

---

## 本工程改动记录：AD 采集部分已换成 `ccs_BC` 那一版

（其余流程、外设、引脚全部没动）

### 改了什么

| 项 | 原来 | 现在 |
| --- | --- | --- |
| 采样窗口 | 3 个周期 × 1024 点 = 3072 点 | **7 个周期 × 512 点 = 3584 点** |
| 定时器周期 `TIMER_ADC_INST` | 19.53 µs | **39.06 µs**（512 点 = 20.000 ms = 1 个工频周期） |
| 取数方式 | 只用中间那 1 个周期 | **每个周期各自算 RMS，去掉最大、去掉最小，剩下 5 个取平均** |
| 读 ADC | 死等标志（定时器不触发就卡死） | **带超时**，超时置 `g_adc_ok = 0`，屏上显示 `ADC ERR` |
| 标定 | 没有 | **`CAL_GAIN = 1.1100f`**（实测标定，见 `ccs_BC\README.md`） |

采样窗口从 60 ms 变成 **140 ms**，比原来多 80 ms，换来的是一点标定后残差 rms 1.3%。

### 没改的（按你的要求）

- AQY412 / AQY212 当时**还是 PA7 一个 IO 口**控制；
  （后来在「第三轮改动」里已拆成 **PA7 → AQY412、PA9 → AQY212** 两个 IO）
- 流程当时还是 **先充电 → 切到测量 → 采一次 → 切回充电 → OLED 上电显示 → 待机**；
  （后来在「第四轮改动」里按实际硬件改成了"测完不切回充电、让电容放完电断电"的循环）
- OLED 显示格式 `ID:%02d I:%.4fA`、蓝牙发送、4 位拨码地址、OLED 的 PA16 电源控制都没动。

### 调试变量（CCS 里挂起程序直接看）

| 变量 | 含义 | 正常值 |
| --- | --- | --- |
| `g_i_rms` | 一次绕组电流有效值 | 与标准表一致 |
| `g_v_rms` | 5 个周期平均后的 Vrms | 2 A 时应约 1.0 V |
| `g_v_dc` | 直流工作点 | **应约 1.65 V**，是 0 说明偏置没建立 |
| `g_cyc_v[0..6]` | 7 个周期各自的 Vrms | 互相之间应该很接近 |
| `g_drop_max/min` | 被舍掉的那两个周期 | 看有没有离群 |
| `g_c_min` / `g_c_max` | 整段窗口的码值范围 | 贴到 0 或 4095 说明削顶 |
| `g_adc_to` | ADC 超时次数 | 应该是 0，>0 说明定时器没触发 |
| `g_adc_ok` | 本轮采集是否完整 | 1 |

---

## 第二轮改动：采样窗口怎么选 + 剔除规则 + 屏上只留一个数

仿真脚本：`F:\Desktop\codex\DS_B\MATLAB\sim_adc_window.m`（跑完出 `sim_adc_window.png`）

### 一、采几个周期 / 每周期几个点 —— 仿真结论

| 方案 | 总点数 | 窗口 | 0.2 A 噪声 | 2.0 A 噪声 |
| --- | --- | --- | --- | --- |
| **7 周期 × 512 点（保持）** | 3584 | 140 ms | 0.021% | 0.0021% |
| 7 周期 × 1024 点 | 7168 | 140 ms | 0.015% | 0.0015% |
| 9 周期 × 512 点 | 4608 | 180 ms | 0.019% | 0.0018% |
| 15 周期 × 512 点 | 7680 | 300 ms | 0.014% | 0.0014% |
| 7 周期 × 256 点 | 1792 | 140 ms | 0.031% | 0.0030% |
| 3 周期 × 1024 点（原来） | 3072 | 60 ms | 0.026% | 0.0026% |

**结论：点数/周期数不是精度的瓶颈。** 现在这个方案的理论噪声只有
0.021%(0.2 A)，改成 1024 点或 15 周期只是把这个已经很小的数再变小一点点
（正好按 1/√总点数 的规律走），对最终那 1% 的误差没有任何意义，
反而多花 RAM（1024 点要 14 KB）和测量时间（15 周期要 300 ms）。

另外还有一条**改点数也消不掉**的系统偏差：工频不是恰好 50.000 Hz 时，
读数会有一个固定偏差 ≈ 半个周期的比例：

| 工频 | 49.90 Hz | 49.95 Hz | 50.00 Hz | 50.10 Hz |
| --- | --- | --- | --- | --- |
| 读数偏差 | +0.099% | +0.050% | +0.001% | −0.100% |

这个和采几个周期、每周期几个点都无关。

### 二、真正有收益的改动：剔除规则

原来的规则是"去掉一个最大值周期，再去掉剩下里面最小的"。
仿真发现它有个致命弱点：**只要有两个周期同时被干扰**（比如继电器刚切换完的
头两个周期），就只剔得掉一个，剩下那个留在平均值里，5 个周期里混进 1 个
+30% 的坏周期 → **读数整体偏高 6%**；坏 3 个就是 12%。

| 场景 | R1 去最大最小（原来） | R3 3×MAD 阈值（现在） |
| --- | --- | --- |
| 干净信号（纯噪声） | +0.0001% ± 0.0023% | +0.0001% ± 0.0023% |
| 中段坏 2 个周期 | **+6.001%** | +0.001% |
| 开头坏 2 个周期 | **+6.001%** | +0.001% |
| 中段坏 3 个周期 | **+12.001%** | +0.001% |
| 1 个偏高 + 1 个偏低 | +0.001% | +0.001% |

R3 = **剔除 |Vrms − 中位数| > 3×MAD 的周期，至少保留 4 个**。
干净数据下噪声和 R1 完全一样，坏周期却能被剔干净。

想换回原来的规则：改 `ccs_empty_B.c` 里的 `#define TRIM_RULE 1U`。
（`2U` = 去掉离中位数最远的 2 个，也是固定剔 2 个。）

### 三、屏上只显示一个数

- 显示内容：**一次绕组电流有效值**，屏幕正中央 2 倍大字，例如 `1.2345 A`；
  采集失败时同一位置显示 `ADC ERR`。不再显示 ID。
- ID 仍然照常读拨码开关、照常通过蓝牙发出去，只是不在屏上占位置。
- **上电只闪一次**：换了 ccs_BC 那一版 OLED 驱动（GRAM + 一次性 `OLED_Flush()`）。
  原来那版是每写一个字节就推一次 I2C，而且 `OLED_Init()` 里面自带一次清屏、
  外面又清一次、再显示一次，所以上电时屏上会明显地跳 3 次。现在只更新一次。
  顺带这一版 I2C 带超时，总线被拉住也不会死等。---

## 第三轮改动：AQY412 / AQY212 拆成两个 IO 口控制

### 一、引脚

> 全部引脚的完整接线（含电源、I2C 上拉、蓝牙、拨码开关、ADC）见 **`引脚接线说明.md`**；
> 继电器部分的元件级原理图见 **`接线说明.md`**。

| 功能 | 引脚 | 上电初值 |
| --- | --- | --- |
| AQY412（常闭，充电支路） | **PA7** | 高 |
| AQY212（常开，测量支路） | **PA9** | 高 |
| OLED 电源（PMOS 栅极） | **PA16** | 高（关断） |
| 蓝牙模块电源（PMOS 栅极） | **PA30** | 高（关断） |
| ADC 输入 | PA15 | — |
| OLED SCL / SDA | PA1 / PA0 | — |
| 蓝牙 RX / TX | PA31 / PA28 | — |
| 拨码开关 bit0~bit3 | PB26 / PA25 / PA23 / PB23 | — |

（上表是本次改位后的实际引脚，`BoosterPack 第 N 脚` 那套排针位置已不再适用，见文件开头的「引脚变更记录」。）

### 二、`Relay_Charge()` / `Relay_Measure()`

元件级接线（原理图 / BOM / 限流电阻怎么选）见同目录 **`接线说明.md`**。
硬件上每个 AQY 只要「1 片芯片 + 1 个 470Ω 限流电阻」，LED 阴极共地。

```c
/* 极性开关: 0 = 高电平点亮 LED(共阴), 1 = 低电平点亮 LED(共阳, 当前配置) */
#define RELAY_ACTIVE_LOW        1

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
#define RELAY_DEADTIME_CYCLES   96000U      /* 3 ms @32MHz */

/* 三处切换都收在这三个函数里, main() 不再自己拼 set/clear */
static void Relay_AllOff(void);   /* MEAS_OFF + CHG_OFF + 3ms  -> 安全态   */
static void Relay_Charge(void);   /* MEAS_OFF -> 3ms -> CHG_ON             */
static void Relay_Measure(void);  /* CHG_OFF  -> 3ms -> MEAS_ON -> 3ms     */
```

`main()` 里的调用点：上电 `Relay_AllOff(); Relay_Charge();` → 测量 `Relay_Measure();`
→ 测完 `Relay_Charge();`。

**当前配置：低电平有效（共阳）** —— `RELAY_ACTIVE_LOW = 1`，硬件是「3.3V 经限流
电阻接 LED 阳极，阴极接 PA7 / PA9」，引脚输出**低**电平时 LED 亮。`.syscfg` 里两个
引脚的 initialValue 已配套改成 `SET`（上电输出高 = LED 灭），否则上电瞬间两个 LED
会先亮一下。细节见 `接线说明.md` 第 11 节。

要换回**高电平有效（共阴）**：把 `RELAY_ACTIVE_LOW` 改回 `0`，并把 `.syscfg` 里
两个引脚的 initialValue 改回 `CLEARED`（共阴接法：PA7 / PA9 经限流电阻接 LED 阳极，
阴极接 GND）。

真值表（低电平有效 / 共阳，`CHG_ON` = PA7 高、`MEAS_ON` = PA9 低）：

| 模式 | PA7 → AQY412 | PA9 → AQY212 | 结果 |
| --- | --- | --- | --- |
| 充电 | 高 → LED 灭 → **闭合** | 高 → LED 灭 → 断开 | 整流桥接上，20Ω 断开 |
| 测量 | 低 → LED 亮 → 断开 | 低 → LED 亮 → **闭合** | 整流桥断开，20Ω 接上 |

**先断后合**：`CHG_OFF() → delay → MEAS_ON()`，`MEAS_OFF() → delay → CHG_ON()`。
绝对不许两路同时闭合，否则整流桥和 20 Ω 一起并到二次绕组上，读数会整体偏小。

### 三、为什么拆

原来一个 IO 串两个 LED：两个 VF 叠加约 2.5 V，3.3 V 轨道上只剩 0.8 V 掉在限流电阻上。
一旦两个 VF 都到 1.5 V 上限，余量只剩 0.3 V，电流掉到 2 mA 以下 —— 低于 AQY 的
must-operate 3 mA，表现就是"有时候切不过去"。拆开后每路单独 3.3 V 驱动，
电流只由限流电阻决定，跟 VF 离散和温度都无关。

**代价**：测量期间两路 LED 必须同时点亮（充电那一路要靠 LED 电流才能保持断开），
总电流从原来的一路约 5 mA 变成两路之和。所以限流电阻不要取得太小：
**推荐 470 Ω（每路约 4 mA，总共 8 mA）**；想更保险就 390 Ω（每路约 4.8 mA）。
不要再小到 220 Ω 以下 —— 采样窗只有 140 ms，但 8800 µF 的电荷预算本来就不宽裕。

### 四、编译结果

无头编译 **0 errors / 4 warnings（版本提示）**：
FLASH `0x4CB8` = 19640 B，SRAM `0x231E` = 8990 B（比上一版多 48 B）。
SysConfig 已生成 `RELAY_PA09_PORT` / `RELAY_PA09_PIN_9_PIN`。

> 注意：仓库里的 `Debug/` 是旧一次构建的产物（里面的 `ti_msp_dl_config.h` 只有
> `RELAY_PA07_*`，没有 `RELAY_PA09_*`）。在 CCS 里编译前先让它重新跑一遍 SysConfig
> （改动过 `.syscfg` 会自动重新生成），否则会报 `RELAY_PA09_PORT` 未定义。

---

## 第四轮改动：按"单次上电 = 单次测量"的实际硬件流程整理

### 一、这一版改了什么

| 项 | 之前 | 现在 |
| --- | --- | --- |
| 上电等待 | `delay_cycles(3200000)` ≈ 100 ms | `POWER_SETTLE_CYCLES` ≈ 10 ms |
| 上电第一步 | `Relay_AllOff(); Relay_Charge();` | **不动继电器**（上电前它就在充电侧） |
| 剔除规则 | `TRIM_RULE = 3`（3×MAD 自适应，剔几个不定） | `TRIM_RULE = 2`（**固定剔 2 个，剩 5 个取平均**） |
| 测完之后 | 立刻 `Relay_Charge()` 切回充电 | 默认**不切回**，停在测量态等电容放完电 |
| 屏上显示 | 只显示数值 | 数值 / `ADC ERR` / **`CLIP`** |

### 二、新时序（常开模式 = 蓝牙一直在线）

由 `ALWAYS_ON_MODE` 选，**默认 1 = 常开模式**：

```
大电容充电, 迟滞翻转, 7533 出 3.3V     -> 单片机开始跑
停到充电端(AQY412 常闭导通, 两路 LED 全灭, 继电器不吃电)
关 OLED(PA16 高) -> 等 POWER_SETTLE_CYCLES（200ms）-> 等偏置/基准稳定
OLED 上电(PA16 低) + 初始化一次
       ↓                                          ↑
   等 MEASURE_INTERVAL_CYCLES（5s, 一直在补电）    |
       ↓                                          |
   先断充电(PA7) -> 3ms -> 合测量(PA9) -> 采 7 个整周期（140ms）
       ↓                                          |
   舍 2 个周期, 剩 5 个取平均 -> 算一次侧电流有效值   |
       ↓                                          |
   立刻切回充电端（两路 LED 灭, 大电容马上补电）      |
       ↓                                          |
   屏上刷新这一个数 + 蓝牙发一帧($T,aa,电流,状态,校验) ┘
```

**因为 3.3V 轨一直不掉，蓝牙模块一直在线、连接不会断**，读表器什么时候来读
都读得到（题目要求每 2 分钟自动采集一次）。

`ALWAYS_ON_MODE = 0` 是老的"单次模式"：测完停在测量端，两路 LED 共 8.7mA 把
大电容抽干 → 单片机断电 → AQY412 常闭触点自动回充电侧 → 下一轮。省电，但每次
上电都要重新建蓝牙连接，读表器基本抓不到。

每次屏上显示的都是**这一轮刚测出来的值**，不是固定值。

蓝牙报文格式（`$T,aa,电流,状态,校验\r\n`）和 4 线接法见 **`引脚接线说明.md` 第 4 节**。

### 三、可以调的开关

| 宏 | 默认 | 含义 |
| --- | --- | --- |
| `ALWAYS_ON_MODE` | `1` | `1` = 常开模式（蓝牙一直在线）；`0` = 单次模式（省电，蓝牙每次重连） |
| `MEASURE_INTERVAL_CYCLES` | `160000000`（5 s @32MHz） | 常开模式下两次测量的间隔，调大更省电 |
| `POWER_SETTLE_CYCLES` | `6400000`（200 ms @32MHz） | 上电后等偏置/基准稳定的时间。想让电容多充一点就调大 |
| `BT_USE_DETECT_PIN` | `0` | `0` = 4 线接法（VCC/GND/RXD/TXD），不接 PB13，直接认为蓝牙在位；`1` = 5 线接法，PB13 接 GND 表示在位 |
| `BT_REPEAT_COUNT` | `0` | 同一帧重发几次：`0` = 一直重发到断电（只在单次模式下有意义）；`>0` = 只重发这么多次 |
| `BT_GAP_CYCLES` | `1600000`（50 ms @32MHz） | 两帧之间的间隔 |

> ⚠️ **切到常开模式前必须先确认两件硬件的事**：
> 1. **AQY212 必须装**（测量支路的固态继电器）。常开模式下大部分时间停在充电端，
>    靠 AQY212 把 20Ω 测量支路从二次绕组上断开；不装的话 PA15 会一直被整流电压顶着。
> 2. **大电容两端必须加限压**（12~15 V TVS／稳压管）。单次模式下 Vcap 被"到了 Von
>    就开机、掉到 Voff 就断电"这个循环压在 4~7 V；常开模式下 3.3 V 侧负载是固定的，
>    大电流时会充得比耗得多，**Vcap 会一直往上冲**（见 `设计说明.md` 第 0.2 节第 4 条）。

### 四、关于"每次都输出 3.5A"

先把算术摆出来（`I1 = Vrms × (200/5)/20 × CAL_GAIN = Vrms × 2.0 × 1.11 = Vrms × 2.22`）：

| 屏上读数 | 对应 `g_v_rms` |
| --- | --- |
| 3.5 A | **≈ 1.58 V** |

再看：如果 ADC 输入被**削顶成方波**（在 0 和 3.3V 之间来回撞），那么
以本周期平均值（1.65V）为零点算出来的 RMS 就是 **1.65 V**，正好对应
`1.65 × 2.22 ≈ 3.66 A`。

所以 **3.5A 基本就是"输入顶到轨"的特征值** —— 20Ω 上不可能有 1.58V 有效值
（2A 时才 1.0V，4A 才 2.0V 而且早该削顶了）。这不是算法算错，是前端/继电器的问题。

**排查顺序**（从最省事的开始）：

1. 挂起看 `g_clip`：**`== 1` 就说明削顶了**，那个 3.5A 是假的。
2. 看 `g_v_dc`：应该是 **1.65V 左右**；不是的话说明偏置/前级不对。
3. 看 `g_c_min` / `g_c_max`：贴到 0 或 4095 就是削顶。
4. 看 `g_cyc_v[0..6]`：7 个周期是不是都差不多大（都大 = 一直削顶）。
5. 示波器直接看 **PA15 对地**：正常应该是骑在 1.65V 上、几十~几百 mV 的小正弦；
   如果是满幅方波 / 50Hz 干扰，就往下面两条查。
6. **最可疑：继电器接线和代码的极性不一致。** 代码刚改成"低电平有效（共阳）"，
   如果板子还是共阴接法，逻辑就整个反了 —— 测量时 20Ω 那一路其实是**断开的**，
   PA15 悬空在拾工频干扰 → 满幅方波 → 3.5A。按 `接线说明.md` 第 11 节检查接线。
7. 其次：**充电支路在测量期间没有真正断开**（整流桥还挂在二次侧），
   20Ω 上看到的是被整流桥钳位的大电压 → 削顶。检查 AQY412 那一路。

### 五、新增的调试变量

| 变量 | 含义 | 正常值 |
| --- | --- | --- |
| `g_clip` | 这一轮 ADC 输入是否贴到轨（削顶） | `0`（`1` = 读数不可信） |
