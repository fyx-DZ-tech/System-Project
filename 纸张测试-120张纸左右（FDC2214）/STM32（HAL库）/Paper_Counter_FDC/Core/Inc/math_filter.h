#ifndef __MATH_FILTER_H
#define __MATH_FILTER_H

#include "main.h"  
// 声明全局变量，让 main.c 能够找到它
extern uint32_t g_Base_RawData;

// 函数声明
uint32_t Get_Stable_FDC_Data(void);
void Calibrate_Base_Data(void);
int Calculate_Paper_Sheets(long long delta); 

#endif
