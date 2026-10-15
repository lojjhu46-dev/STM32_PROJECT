/*!
 * \file      sx1262mbxcas-board.c
 *
 * \brief     Target board SX1262MBXCAS shield driver implementation
 *
 * \copyright Revised BSD License, see section \ref LICENSE.
 *
 * \code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2013-2017 Semtech
 *
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 */
#include <stdlib.h>
#include "project_config.h"
#include "delay.h"
#include "radio.h"
#include "LLCC68-board.h"
//#include "HAL_SPI.h"
#include "stdio.h"
#include "main.h"
#include "spi.h"
#include "tim.h"
typedef struct __softTimer_s{
	uint8_t isRun;		//0停止，1运行
	uint32_t delayMs;	//定时器需要多长时间后唤醒
	uint32_t startMs;	//定时器开始时间
}SoftTimer_t;

static DioIrqHandler *dio1IrqCallback=NULL;	//记录DIO1的回调函数句柄
static uint32_t timer_count=0;
static SoftTimer_t txTimerHandle,rxTimerHandle;

/* 初始化LLCC68需要用到的GPIO初始化，将 BUSY 引脚设置为输入模式
 */
void LLCC68IoInit( void )
{

}

/* 初始化 DIO1
 * 将DIO1设置为外部中断(上升沿触发)，并且回调函数为 dioIrq 函数原型 void RadioOnDioIrq( void* context )
 */
void LLCC68IoIrqInit( DioIrqHandler dioIrq )
{
	dio1IrqCallback=dioIrq;

}

////void LLCC68IoIrqInit( DioIrqHandler dioIrq )
//void EXTI1_IRQHandler(void){
//	if(GPIO_ReadInputDataBit(RADIO_DIO1_PORT,RADIO_DIO1_PIN)){
//		printf("DIO1 irq\r\n");
//		if(NULL!=dio1IrqCallback){
//			dio1IrqCallback(NULL);
//		}
//		EXTI_ClearITPendingBit(EXTI_Line1);
//	}
//}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	printf("DIO1 irq\r\n");
	if(NULL!=dio1IrqCallback)
	{
			dio1IrqCallback(NULL);
	}
}


void LLCC68IoDeInit( void )
{
    //GPIO去初始化代码
}

void LLCC68ResetInit( void )
{
	delay_ms(10);
	
	//拉低RESET
	HAL_GPIO_WritePin(LORA_RESET_GPIO_Port, LORA_RESET_Pin, GPIO_PIN_RESET);
	
	delay_ms(20);
	
	//拉高RESET
	HAL_GPIO_WritePin(LORA_RESET_GPIO_Port, LORA_RESET_Pin, GPIO_PIN_SET);
	
	delay_ms(20);
}

//读取Busy引脚电平状态
void LLCC68WaitOnBusy( void )
{
	uint32_t u32_count=0;
	
	while( HAL_GPIO_ReadPin(LORA_BUSY_GPIO_Port,LORA_BUSY_Pin) == 1 ){
		if(u32_count++>1000){
			printf("wait busy pin timeout\r\n");
			u32_count=0;
		}
		LLCC68DelayMs(1);
	}
}

//设置SPI的NSS引脚电平，0低电平，非零高电平
void LLCC68SetNss(uint8_t lev ){
	if(lev){
		HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_SET);
	}else{
		HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_RESET);
	}
}

//spi传输一个字节的数据
uint8_t LLCC68SpiInOut(uint8_t data)
{
	uint8_t rxdata = 0;
	HAL_SPI_TransmitReceive(&hspi1,&data,&rxdata,1,1000);
	return rxdata;
}

//检查频率是否符合要求，如果不需要判断则可以直接返回true
bool LLCC68CheckRfFrequency( uint32_t frequency )
{
    // Implement check. Currently all frequencies are supported
    return true;
}

//毫秒延时
void LLCC68DelayMs(uint32_t ms){
	delay_ms(ms);
}

//tx/rx定时器操作

//定时器3中断服务程序
//tx定时结束需要回调 RadioOnTxTimeoutIrq
//rx定时结束需要回调 RadioOnRxTimeoutIrq
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	uint32_t diffMs=0;
	
	if(htim->Instance == TIM3)
	{
		timer_count++;
		//处理tx定时器
		if(txTimerHandle.isRun){
			if(timer_count>=txTimerHandle.startMs){
				diffMs=timer_count-txTimerHandle.startMs;
			}else{//溢出了
				diffMs=0xffffffff - txTimerHandle.startMs + timer_count;
			}
			if(diffMs>txTimerHandle.delayMs){
				//计时结束
				//printf("----------------- tx timer irq ---------------\r\n");
				LLCC68TxTimerStop();
				txTimerHandle.startMs=timer_count;
				RadioOnTxTimeoutIrq(NULL);	//tx定时结束需要回调 RadioOnTxTimeoutIrq
			}
		}
		
		//处理rx定时器
		if(rxTimerHandle.isRun){
			if(timer_count>=rxTimerHandle.startMs){
				diffMs=timer_count-rxTimerHandle.startMs;
			}else{//溢出了
				diffMs=0xffffffff - rxTimerHandle.startMs + timer_count;
			}
			if(diffMs>rxTimerHandle.delayMs){
				//计时结束
				//printf("----------------- rx timer irq ---------------\r\n");
				LLCC68RxTimerStop();
				rxTimerHandle.startMs=timer_count;
				RadioOnRxTimeoutIrq(NULL);	//rx定时结束需要回调 RadioOnRxTimeoutIrq
			}
		}
	}


}
//void TIM3_IRQHandler(void)   //TIM3中断
//{
//	uint32_t diffMs=0;
//	
//	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET){
//		timer_count++;
//		TIM_ClearITPendingBit(TIM3, TIM_IT_Update  );  //清除TIMx更新中断标志
//		
//		//printf("timer_count=%d\r\n",timer_count);
//		
//		//处理tx定时器
//		if(txTimerHandle.isRun){
//			if(timer_count>=txTimerHandle.startMs){
//				diffMs=timer_count-txTimerHandle.startMs;
//			}else{//溢出了
//				diffMs=0xffffffff - txTimerHandle.startMs + timer_count;
//			}
//			if(diffMs>txTimerHandle.delayMs){
//				//计时结束
//				//printf("----------------- tx timer irq ---------------\r\n");
//				LLCC68TxTimerStop();
//				txTimerHandle.startMs=timer_count;
//				RadioOnTxTimeoutIrq(NULL);	//tx定时结束需要回调 RadioOnTxTimeoutIrq
//			}
//		}
//		
//		//处理rx定时器
//		if(rxTimerHandle.isRun){
//			if(timer_count>=rxTimerHandle.startMs){
//				diffMs=timer_count-rxTimerHandle.startMs;
//			}else{//溢出了
//				diffMs=0xffffffff - rxTimerHandle.startMs + timer_count;
//			}
//			if(diffMs>rxTimerHandle.delayMs){
//				//计时结束
//				//printf("----------------- rx timer irq ---------------\r\n");
//				LLCC68RxTimerStop();
//				rxTimerHandle.startMs=timer_count;
//				RadioOnRxTimeoutIrq(NULL);	//rx定时结束需要回调 RadioOnRxTimeoutIrq
//			}
//		}
//	}
//}

//初始化定时器(这里用TIM3定时器模拟出了两个ms定时器)
//tx定时结束需要回调 RadioOnTxTimeoutIrq
//rx定时结束需要回调 RadioOnRxTimeoutIrq
void LLCC68TimerInit(void){
	
	//启用定时器
	HAL_TIM_Base_Start_IT(&htim3);
	
	//关闭软件定时器
	txTimerHandle.isRun=0;
	rxTimerHandle.isRun=0;
	
	
}

void LLCC68SetTxTimerValue(uint32_t nMs){
	printf("[%s()-%d]set timer out %d ms\r\n",__func__,__LINE__,nMs);
	txTimerHandle.delayMs=nMs;
}

void LLCC68TxTimerStart(void){
	printf("[%s()-%d]start timer\r\n",__func__,__LINE__);
	txTimerHandle.startMs=timer_count;
	txTimerHandle.isRun=1;
}

void LLCC68TxTimerStop(void){
	printf("[%s()-%d]stop timer\r\n",__func__,__LINE__);
	txTimerHandle.isRun=0;
}

void LLCC68SetRxTimerValue(uint32_t nMs){
	printf("[%s()-%d]set timer out %d ms\r\n",__func__,__LINE__,nMs);
	rxTimerHandle.delayMs=nMs;
}

void LLCC68RxTimerStart(void){
	printf("[%s()-%d]start timer\r\n",__func__,__LINE__);
	rxTimerHandle.startMs=timer_count;
	rxTimerHandle.isRun=1;
}

void LLCC68RxTimerStop(void){
	printf("[%s()-%d]stop timer\r\n",__func__,__LINE__);
	rxTimerHandle.isRun=0;
}
