#ifndef __OLED_H
#define __OLED_H

#include "stdint.h"

// OLED 初始化与清屏函数声明
void OLED_Init(void);
void OLED_Clear(void);

// OLED 显示核心函数声明
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
// 在你原有的声明下面加上这两句
void OLED_ShowBigChar(uint8_t X, char Char);
void OLED_ShowBigNum(uint8_t X, uint32_t Number, uint8_t Length);

#endif
