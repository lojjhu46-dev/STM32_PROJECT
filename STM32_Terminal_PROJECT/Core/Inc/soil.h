#include "main.h"
#include "adc.h"
#include "usart.h"
#include "gpio.h"

#include <stdio.h>

void SOIL_GetValue(void);

//ADC采集结果（定义于 soil.c），供 main 获取光照与土壤湿度原始值
//SOIL_GetValue() 一次调用同时完成两路采集：ldr_val 为光照(CH0)，soil_val 为土壤湿度(CH1)
extern uint16_t soil_val;
extern uint16_t ldr_val;