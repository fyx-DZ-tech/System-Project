#ifndef __OLED_H
#define __OLED_H

#include "ti_msp_dl_config.h"

extern volatile uint8_t  g_oled_addr;
extern volatile uint32_t g_i2c_to_cnt;

uint8_t OLED_Probe(void);
void OLED_Init(void);
void OLED_Clear(void);
void OLED_PowerOff(void);          /* 关显示 + 清显存, 再切电源轨可避免雪花 */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowString2X(uint8_t Page, uint8_t Column, char *String);
void OLED_Flush(void);
uint8_t OLED_WriteBytes(const uint8_t *src, uint16_t n);   /* 1 = 全部发出, 0 = 失败 */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);

#endif