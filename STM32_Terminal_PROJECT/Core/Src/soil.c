#include "soil.h"

uint16_t soil_val = 0;
uint16_t ldr_val = 0;

/* 读取单个ADC通道的转换值（单通道切换方式）
 *
 * 注意：STM32F103C8属于中密度(Medium-density)器件，ADC_CR2的EOCS位无效，
 * 扫描模式下若不使用DMA，EOC只在整个转换序列结束时才置位，无法逐个读取多个通道，
 * 会导致两次读取拿到同一个结果。因此这里每次只配置并转换一个通道。
 *
 * 参数：channel - ADC_CHANNEL_0(PA0 光照) 或 ADC_CHANNEL_1(PA1 土壤湿度)
 * 返回：转换结果 0~4095，超时则返回 0
 */
static uint16_t ADC_ReadChannel(uint32_t channel)
{
	ADC_ChannelConfTypeDef sConfig = {0};
	uint16_t value = 0;
	
	//1.配置本次要转换的通道（单通道，Rank固定为第1个）
	sConfig.Channel      = channel;
	sConfig.Rank         = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;	//光敏电阻/土壤探头输出阻抗高，需长采样时间
	if( HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK )
	{
		return 0;
	}
	
	//2.启动转换并等待完成（单次转换约21us，10ms超时余量充足）
	HAL_ADC_Start(&hadc1);
	if( HAL_ADC_PollForConversion(&hadc1,10) == HAL_OK )
	{
		value = (uint16_t)HAL_ADC_GetValue(&hadc1);
	}
	HAL_ADC_Stop(&hadc1);
	
	return value;
}

/* 采集光照(CH0/PA0)与土壤湿度(CH1/PA1)，结果存入全局变量 ldr_val / soil_val */
void SOIL_GetValue(void)
{
	//1.采集光照强度  PA0 -> ADC_CHANNEL_0
	ldr_val = ADC_ReadChannel(ADC_CHANNEL_0);
	
	//2.采集土壤湿度  PA1 -> ADC_CHANNEL_1
	soil_val = ADC_ReadChannel(ADC_CHANNEL_1);
	
	//3.输出转换结果（附加电压值mV，便于判断传感器与接线是否正常）
	printf("ldr val = %d (%.0f mV), soil val = %d (%.0f mV)\r\n",
	       ldr_val,  ldr_val  * 3300.0f / 4095.0f,
	       soil_val, soil_val * 3300.0f / 4095.0f);
}
