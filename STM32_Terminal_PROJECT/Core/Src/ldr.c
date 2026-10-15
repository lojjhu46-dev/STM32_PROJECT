#include "ldr.h"

void LDR_GetValue(void)
{
		uint16_t ldr_val = 0;
    //开启ADC转换
		HAL_ADC_Start(&hadc1);   
		
		//等待转换完成
		while( HAL_ADC_PollForConversion(&hadc1,1000) != HAL_OK );
		
		//获取转换结果  0~4095
		ldr_val = HAL_ADC_GetValue(&hadc1);
		
		//输出转换结果
		printf("ldr val = %d\r\n",ldr_val);
		
		HAL_Delay(500);
}