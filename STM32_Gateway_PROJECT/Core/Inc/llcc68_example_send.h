#ifndef __LLCC68_EXAMPLE_SEND_H__
#define __LLCC68_EXAMPLE_SEND_H__

#include <stdint.h>

void ExampleLLCC68SendDemo(void);

/* 非阻塞接口，供 main.c 业务主循环调用 */
void    LLCC68_RadioInit(void);									//初始化Radio并配置射频参数
uint8_t LLCC68_SendPacket(uint8_t *data, uint8_t len);	//发送一包，返回1成功启动、0总线忙
void    LLCC68_IrqProcess(void);								//主循环轮询处理LoRa中断事件

//发送完成标志：1=空闲可发送，0=正在发送
extern volatile uint8_t loraTxDone;

#endif //end of __LLCC68_EXAMPLE_SEND_H__
