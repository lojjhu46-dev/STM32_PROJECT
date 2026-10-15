#include "delay.h"

#include "tim.h"

//延时微秒
void delay_us(uint32_t nus)
{
	//1.重置计数器
	__HAL_TIM_SET_COUNTER(&htim2,0);

	//2.等待时间到达
	while(__HAL_TIM_GET_COUNTER(&htim2)<nus);
}

//延时毫秒
void delay_ms(uint16_t nms)
{
	while(nms--)
	{
		delay_us(1000);
	}

}
