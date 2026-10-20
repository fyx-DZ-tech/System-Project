#include "delay.h"

static u8  fac_us = 0; // us延时倍乘数			   
static u16 fac_ms = 0; // ms延时倍乘数, 在os下,代表每个节拍的ms数

// 初始化延迟函数
// 当使用 SysTick 的时候,如果使用OS,一定要调用此函数!
// SYSTICK 的时钟固定为 HCLK 时钟的 1/8 (对于 STM32F103 通常是 72M/8 = 9M)
void delay_init(void)
{
    // 配置 SysTick 时钟源为 HCLK/8
    SysTick->CTRL &= ~(1 << 2); 
    
    // fac_us = SystemCoreClock / 8000000; // 理论上这么写，但为了兼容性直接写死：
    fac_us = 9;   // STM32F103 主频 72MHz 时，9 个 SysTick 周期即为 1us
    fac_ms = (u16)fac_us * 1000; // 1ms = 1000us
}

// 微秒级延时 (nus)
// nus的值,不要大于 798915us (最大值即 2^24 / 9 / 1000000)
void delay_us(u32 nus)
{		
    u32 temp;	    	 
    SysTick->LOAD = nus * fac_us;             // 时间加载	  		 
    SysTick->VAL = 0x00;                      // 清空计数器
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk; // 开始倒数	  
    do
    {
        temp = SysTick->CTRL;
    } while((temp & 0x01) && !(temp & (1 << 16))); // 等待时间到达   
    
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk; // 关闭计数器
    SysTick->VAL = 0X00;                       // 清空计数器	 
}

// 毫秒级延时 (nms)
// nms 的范围
// SysTick->LOAD 为 24位寄存器,所以,最大延时为:
// nms <= 0xffffff * 8 * 1000 / SYSCLK
// 对于 72M 条件下, nms <= 1864 
void delay_ms(u16 nms)
{	 		  	  
    u32 temp;		   
    SysTick->LOAD = (u32)nms * fac_ms;        // 时间加载(SysTick->LOAD为24bit)
    SysTick->VAL = 0x00;                      // 清空计数器
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk; // 开始倒数  
    do
    {
        temp = SysTick->CTRL;
    } while((temp & 0x01) && !(temp & (1 << 16))); // 等待时间到达   
    
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk; // 关闭计数器
    SysTick->VAL = 0X00;                       // 清空计数器	  	    
}