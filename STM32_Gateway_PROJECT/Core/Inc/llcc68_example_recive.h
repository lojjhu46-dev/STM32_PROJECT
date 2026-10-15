#ifndef __LLCC68_EXAMPLE_RECIVE_H__
#define __LLCC68_EXAMPLE_RECIVE_H__

#include <stdint.h>

void ExampleLLCC68ReciveDemo(void);

/* 非阻塞接收接口，供 main.c 业务主循环调用（与 llcc68_example_send.h 的风格对称） */
void    LLCC68_RecvInit(void);			//注册回调 + 配置射频参数 + 进入连续接收
void    LLCC68_RecvIrqProcess(void);	//主循环高频调用：处理DIO1中断事件
uint8_t LLCC68_RecvGetPacket(char *buf, uint8_t bufLen,
                             int16_t *rssi, int8_t *snr);	//取出一包，返回数据长度(0=无新包)

//接收标志：1=收到新包待main取走（取走时自动清零）
extern volatile uint8_t loraRxFlag;

#endif //end of __LLCC68_EXAMPLE_RECIVE_H__
