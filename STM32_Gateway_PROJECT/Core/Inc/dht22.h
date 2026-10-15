#include <stdio.h>
#include <stdbool.h>
#include "main.h"
#include "usart.h"
#include "tim.h"

void DHT22_SetPinMode(uint32_t Mode);
//开始信号
void DHT22_Start(void);
//等待应答  
bool DHT22_WaitACK(void);
//读取bit  0x00(0000000 0) 表示位数据0    0x01(0000000 1)  表示位数据1
uint8_t DHT22_ReadBit(void);
//读取字节
uint8_t DHT22_ReadByte(void);
bool DHT22_GetVal(float *temp,float *humi);