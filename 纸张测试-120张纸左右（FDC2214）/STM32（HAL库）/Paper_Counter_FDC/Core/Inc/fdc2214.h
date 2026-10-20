#ifndef __FDC2214_H
#define __FDC2214_H

#include "main.h"

// FDC2214 I2C 地址 (ADDR引脚接GND，7位地址0x2A左移一位为0x54)
#define FDC2214_ADDR 0x54

uint8_t FDC2214_Init(void);
uint32_t FDC2214_ReadCH0_Raw(void);
float FDC2214_Get_Frequency(void); // 可选：转换为频率值

#endif
