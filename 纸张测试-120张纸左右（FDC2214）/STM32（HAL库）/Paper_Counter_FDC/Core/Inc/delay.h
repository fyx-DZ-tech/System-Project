#ifndef __DELAY_H
#define __DELAY_H 			   

#include "sys.h"  // 包含系统头文件以识别 u8, u16, u32 等类型
// 如果你在 HAL 库下没有 sys.h，请改为 #include "main.h"

void delay_init(void);
void delay_ms(u16 nms);
void delay_us(u32 nus);

#endif