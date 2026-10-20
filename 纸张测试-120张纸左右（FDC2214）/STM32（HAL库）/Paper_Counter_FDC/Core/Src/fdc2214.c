#include "fdc2214.h"
#include "main.h" 

// 软件 I2C 引脚定义
#define FDC_SCL_PIN  GPIO_PIN_6
#define FDC_SDA_PIN  GPIO_PIN_7
#define FDC_PORT     GPIOB

#define FDC_SCL(x)   HAL_GPIO_WritePin(FDC_PORT, FDC_SCL_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define FDC_SDA(x)   HAL_GPIO_WritePin(FDC_PORT, FDC_SDA_PIN, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define FDC_SDA_READ() HAL_GPIO_ReadPin(FDC_PORT, FDC_SDA_PIN)

// 默认 ADDR 接地。如果你的 PCB 上 ADDR 接了 3.3V，请把下面两行改为 0x56 和 0x57
#define FDC2214_ADDR_W 0x54
#define FDC2214_ADDR_R 0x55

static void FDC_Delay(void) {
    volatile uint8_t i = 100; // 稳定降速，确保板子走线电容不影响时序，必须有 volatile
    while(i--);
}

static void FDC_I2C_Start(void) {
    FDC_SDA(1); FDC_SCL(1); FDC_Delay();
    FDC_SDA(0); FDC_Delay();
    FDC_SCL(0); FDC_Delay();
}

static void FDC_I2C_Stop(void) {
    FDC_SDA(0); FDC_SCL(0); FDC_Delay();
    FDC_SCL(1); FDC_Delay();
    FDC_SDA(1); FDC_Delay();
}

static uint8_t FDC_I2C_WaitAck(void) {
    uint8_t ack;
    FDC_SDA(1); FDC_Delay();
    FDC_SCL(1); FDC_Delay();
    ack = FDC_SDA_READ();
    FDC_SCL(0); FDC_Delay();
    return ack;
}

static void FDC_I2C_Ack(void) {
    FDC_SCL(0); FDC_SDA(0); FDC_Delay();
    FDC_SCL(1); FDC_Delay();
    FDC_SCL(0); FDC_Delay();
}

static void FDC_I2C_NAck(void) {
    FDC_SCL(0); FDC_SDA(1); FDC_Delay();
    FDC_SCL(1); FDC_Delay();
    FDC_SCL(0); FDC_Delay();
}

static void FDC_I2C_SendByte(uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        FDC_SDA((byte & 0x80) >> 7);
        byte <<= 1;
        FDC_Delay();
        FDC_SCL(1); FDC_Delay();
        FDC_SCL(0); FDC_Delay();
    }
}

static uint8_t FDC_I2C_ReadByte(uint8_t ack) {
    uint8_t byte = 0;
    FDC_SDA(1); 
    for (uint8_t i = 0; i < 8; i++) {
        byte <<= 1;
        FDC_SCL(1); FDC_Delay();
        if (FDC_SDA_READ()) byte |= 0x01;
        FDC_SCL(0); FDC_Delay();
    }
    if (ack) FDC_I2C_Ack();
    else FDC_I2C_NAck();
    return byte;
}

// 优化：写寄存器函数增加错误返回检测机制
static uint8_t FDC2214_WriteReg(uint8_t reg, uint16_t value) {
    FDC_I2C_Start();
    FDC_I2C_SendByte(FDC2214_ADDR_W);
    if (FDC_I2C_WaitAck()) goto I2C_FAIL; // 无应答跳转
    
    FDC_I2C_SendByte(reg);
    if (FDC_I2C_WaitAck()) goto I2C_FAIL;
    
    FDC_I2C_SendByte((value >> 8) & 0xFF); 
    if (FDC_I2C_WaitAck()) goto I2C_FAIL;
    
    FDC_I2C_SendByte(value & 0xFF);        
    if (FDC_I2C_WaitAck()) goto I2C_FAIL;
    
    FDC_I2C_Stop();
    return 0; // 成功

I2C_FAIL:
    FDC_I2C_Stop();
    return 1; // 失败
}

static uint16_t FDC2214_ReadReg(uint8_t reg) {
    uint16_t value = 0;
    FDC_I2C_Start();
    FDC_I2C_SendByte(FDC2214_ADDR_W);
    FDC_I2C_WaitAck(); // 这里简化处理，读操作一般连上了就不会断
    FDC_I2C_SendByte(reg);
    FDC_I2C_WaitAck();

    FDC_I2C_Start();
    FDC_I2C_SendByte(FDC2214_ADDR_R);
    FDC_I2C_WaitAck();
    value = FDC_I2C_ReadByte(1); 
    value <<= 8;
    value |= FDC_I2C_ReadByte(0); 
    FDC_I2C_Stop();
    return value;
}

// 优化：返回 uint8_t，支持 main 函数里判断芯片是否在线
uint8_t FDC2214_Init(void) {
    FDC_SCL(1); FDC_SDA(1); HAL_Delay(10);
    
    // 开机检测：读制造商 ID 寄存器 (0x7E 应返回 0x5449)
    uint16_t dev_id = FDC2214_ReadReg(0x7E);
    if (dev_id != 0x5449) {
        return 1; // 1 表示失败：芯片不在线、IIC线断了或地址错误
    }
    
    FDC2214_WriteReg(0x1C, 0x8000); // 软复位
    HAL_Delay(50);
    
    FDC2214_WriteReg(0x08, 0xFFFF); // RCOUNT: 转换时间拉满，提供最强抗噪
    FDC2214_WriteReg(0x10, 0x0400); // SETTLECOUNT: 适合100uH的建立时间
    FDC2214_WriteReg(0x14, 0x1001); // CLOCK_DIVIDERS: 1分频
    FDC2214_WriteReg(0x1E, 0x7800); // DRIVE_CURRENT: 适配100uH+36pF的保守驱动电流
    FDC2214_WriteReg(0x1B, 0x020D); // MUX_CONFIG: 单通道连续读取
    
    // 注意：如果有外部40M晶振用 0x1E81，如果没有(用内部振荡器)请改成 0x1481
    FDC2214_WriteReg(0x1A, 0x1E81); 
    
    HAL_Delay(20);
    return 0; // 0 表示初始化完美成功
}

// 优化：加入数据就绪 (DRDY) 轮询，避免读到重复的旧数据
uint32_t FDC2214_ReadCH0_Raw(void) {
    uint16_t status = 0;
    uint16_t timeout = 500; // 超时计数器，防止硬件死机导致程序死循环
    
    // 轮询等待 STATUS 寄存器 (0x18) 的第 3 位变成 1
    do {
        status = FDC2214_ReadReg(0x18);
        timeout--;
        FDC_Delay(); 
    } while (((status & 0x0008) == 0) && (timeout > 0));
    
    if (timeout == 0) return 0; // 超时返回0，上层可借此判断异常
    
    // 数据已准备好，安全读取
    uint16_t msb = FDC2214_ReadReg(0x00);
    uint16_t lsb = FDC2214_ReadReg(0x01);
    
    return (((uint32_t)(msb & 0x0FFF)) << 16) | lsb; 
}

