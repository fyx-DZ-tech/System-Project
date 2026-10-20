#include "math_filter.h"
#include "fdc2214.h"
#include "main.h" 

// 全局基准变量（空载去皮数据），用于彻底抵消环境漂移
uint32_t g_Base_RawData = 0; 

// =======================================================
// 优化后的 3 段式 3 阶多项式系数 (已针对 100 张量程进行拟合)
// =======================================================

// === 自动生成的 3段式多项式系数 (C语言格式) ===

// 【区间 1】(Delta < -1571441)
#define Z1_COEFF_A  3.180497e-08f
#define Z1_COEFF_B  8.835564e-05f
#define Z1_COEFF_C  8.765276e-02f
#define Z1_COEFF_D  3.387522e+01f

// 【区间 2】(Delta >= -1571441 且 < 86851)
#define Z2_COEFF_A  3.427046e-06f
#define Z2_COEFF_B  1.715298e-03f
#define Z2_COEFF_C  3.703459e-01f
#define Z2_COEFF_D  5.229681e+01f

// 【区间 3】(Delta >= 86851)
#define Z3_COEFF_A  3.153817e-04f
#define Z3_COEFF_B  -1.480195e-02f
#define Z3_COEFF_C  7.791096e-01f
#define Z3_COEFF_D  4.979177e+01f




/*
 * 纯数学映射：3段式 3 阶多项式算法
 */
int Calculate_Paper_Sheets(long long delta) {
    
    // 基础防溢出保护
    if (delta < -13000000) return 0;
    
    float x = (float)delta / 10000.0f;
    float result = 0.0f;

    // 分段点更新为 -1571441 和 86851
    if (delta < -1571441) {
        // 区间 1
        result = (Z1_COEFF_A * x * x * x) + (Z1_COEFF_B * x * x) + (Z1_COEFF_C * x) + Z1_COEFF_D;
    } else if (delta < 86851) {
        // 区间 2
        result = (Z2_COEFF_A * x * x * x) + (Z2_COEFF_B * x * x) + (Z2_COEFF_C * x) + Z2_COEFF_D;
    } else {
        // 区间 3
        result = (Z3_COEFF_A * x * x * x) + (Z3_COEFF_B * x * x) + (Z3_COEFF_C * x) + Z3_COEFF_D;
    }

    int final_sheets = (int)(result + 0.5f);
    
    if (final_sheets < 0) final_sheets = 0;
    if (final_sheets > 105) final_sheets = 105; 

    return final_sheets;
}

uint32_t Get_Stable_FDC_Data(void)
{
    // 最简实现：直接返回原始数据（能编译通过，后续再优化滤波）
    return FDC2214_ReadCH0_Raw();
}

void Calibrate_Base_Data(void)
{
    // 读取 20 次求平均，获得极其纯净的现场环境零点数据
    uint64_t sum = 0;
    for(int i = 0; i < 20; i++) {
        sum += FDC2214_ReadCH0_Raw();
        HAL_Delay(5);
    }
    g_Base_RawData = (uint32_t)(sum / 20);
}
