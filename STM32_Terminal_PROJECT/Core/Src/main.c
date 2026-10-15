/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "string.h"
#include "dht22.h"
#include "mqtt.h"
#include "oled.h"
#include "soil.h"
#include "LLCC68_example_recive.h"
#include "LLCC68_example_send.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SAMPLE_PERIOD_MS	5000	//采集与上报周期(ms)，5秒一轮
#define MQTT_RETRY_MS		30000	//MQTT掉线重连的最小间隔(ms)
#define ADC_MIN_VALID		1		//ADC有效下限，读数为0视为传感器未接
#define ADC_MAX_VALID		4094	//ADC有效上限，读数为4095视为短路/未接
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
float temp = 0.0f;					//空气温度(℃)
float humi = 0.0f;					//空气湿度(%RH)
uint8_t dht_ok = 0;					//温湿度采集成功标志
uint8_t adc_ok = 0;					//光照/土壤采集成功标志
uint8_t mqtt_ok = 0;				//已连接巴法云MQTT标志
uint8_t lora_ok = 0;				//LoRa模块就绪标志
uint32_t lastSampleTick = 0;		//上次采集时刻
uint32_t lastMqttRetryTick = 0;		//上次MQTT重连时刻
char payload[64];					//MQTT与LoRa共用的数据帧缓冲
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void SystemClock_Config(void);

//对fputc进行重定向，用于支持printf

int fputc(int ch, FILE *f) 
{
	HAL_UART_Transmit(&huart1,(uint8_t *)&ch,1,1000);
	return ch;
}

/* 计算总状态：2=全部成功(ALL OK)  1=部分失败(PART FAIL)  0=全部失败(FAIL) */
//static uint8_t AGRI_CalcStatus(uint8_t dhtSta,uint8_t adcSta,uint8_t mqttSta)
//{
//	uint8_t okCnt = 0;
//	
//	if(dhtSta)  okCnt++;
//	if(adcSta)  okCnt++;
//	
//	if(okCnt == 3)	return 2;	//三项都成功
//	if(okCnt == 0)	return 0;	//三项都失败
//	return 1;					//部分失败
//}
static uint8_t AGRI_CalcStatus(uint8_t dhtSta,uint8_t adcSta)
{
	uint8_t okCnt = 0;
	
	if(dhtSta)  okCnt++;
	if(adcSta)  okCnt++;
	
	if(okCnt == 2)	return 2;	//两项都成功
	if(okCnt == 0)	return 0;	//两项都失败
	return 1;					//部分失败
	
}

/* 刷新OLED：第1行温湿度、第2行光照与土壤、第3行模块状态、第4行总状态
   注意：每行最多容纳16个字符(128px/8px) */
//static void AGRI_OledRefresh(float t,float h,uint16_t ldr,uint16_t soil,
//                             uint8_t dhtSta,uint8_t adcSta,uint8_t mqttSta,
//                             uint8_t loraSta,uint8_t totalSta)
static void AGRI_OledRefresh(float t,float h,uint16_t ldr,uint16_t soil,
                             uint8_t dhtSta,uint8_t adcSta,
                             uint8_t loraSta,uint8_t totalSta)
{
	char buf[17];
	
	OLED_Clear();
	
	//第1行：空气温湿度
	if(dhtSta){ sprintf(buf,"T:%.1fC",t); }else{ sprintf(buf,"T:--"); }
	OLED_ShowString(0,0,(uint8_t *)buf,16);
	if(dhtSta){ sprintf(buf,"H:%.1f%%",h); }else{ sprintf(buf,"H:--"); }
	OLED_ShowString(64,0,(uint8_t *)buf,16);
	
	//第2行：光照强度与土壤湿度
	if(adcSta){ sprintf(buf,"L:%d",ldr); }else{ sprintf(buf,"L:--"); }
	OLED_ShowString(0,2,(uint8_t *)buf,16);
	if(adcSta){ sprintf(buf,"S:%d",soil); }else{ sprintf(buf,"S:--"); }
	OLED_ShowString(64,2,(uint8_t *)buf,16);
	
	//第3行：MQTT与LoRa模块状态
	//OLED_ShowString(0,4,(uint8_t *)(mqttSta ? "MQ:OK" : "MQ:NG"),16);
	OLED_ShowString(64,4,(uint8_t *)(loraSta ? "LORA:OK" : "LORA:NG"),16);
	
	//第4行：总状态
	switch(totalSta)
	{
		case 2:  sprintf(buf,"STATUS:ALL OK");	 break;
		case 1:  sprintf(buf,"STATUS:PART FAIL");break;
		default: sprintf(buf,"STATUS:FAIL");	 break;
	}
	OLED_ShowString(0,6,(uint8_t *)buf,16);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
	HAL_TIM_Base_Start(&htim2);  	//启动TIM2，供delay_us/delay_ms使用
	
	//1.OLED初始化，显示开机画面
	OLED_Init();
	OLED_Clear();
	OLED_ShowString(0,0,(uint8_t *)"AGRI MONITOR",16);
	OLED_ShowString(0,2,(uint8_t *)"BOOTING...",16);
	printf("\r\n====== AGRI MONITOR START ======\r\n");
	
	//2.启动UART2空闲中断接收，必须在ESP_Init之前，否则收不到ESP8266响应
	HAL_UARTEx_ReceiveToIdle_IT(&huart2,wifi_rxtemp,BUFFERSIZE);
	
	//3.LoRa初始化（配置射频参数，非阻塞）
	OLED_ShowString(0,4,(uint8_t *)"LORA INIT",16);
	LLCC68_RadioInit();
	lora_ok = 1;
	
//	//4.连接巴法云：ESP8266连热点与服务器 -> MQTT连接(clientid填私钥) -> 订阅主题
//	OLED_ShowString(0,6,(uint8_t *)"WIFI CONNECT",16);
//	if( ESP_Init() )
//	{
//		printf("[ESP]init ok\r\n");
//		if( MQTT_Connect((char *)USER_KEY) )
//		{
//			printf("[MQTT]connect ok\r\n");
//			if( MQTT_Subscribe((char *)TOPIC,MQTT_QoS0) )
//			{
//				printf("[MQTT]subscribe ok\r\n");
//			}
//			mqtt_ok = 1;
//		}
//		else
//		{
//			printf("[MQTT]connect fail\r\n");
//		}
//	}
//	else
//	{
//		printf("[ESP]init fail\r\n");
//	}
	
	lastSampleTick     = HAL_GetTick();
	lastMqttRetryTick  = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
		
    /* USER CODE BEGIN 3 */
	//1.高频处理LoRa中断事件，保证DIO1事件及时响应（非阻塞）
	LLCC68_IrqProcess();
	
	//2.到达5秒周期：采集 -> 拼帧 -> MQTT上报 -> LoRa发送 -> OLED刷新
	if( HAL_GetTick() - lastSampleTick >= SAMPLE_PERIOD_MS )
	{
		lastSampleTick = HAL_GetTick();
		
		//2.1 采集空气温湿度
		dht_ok = DHT22_GetVal(&temp,&humi) ? 1 : 0;
		
		//2.2 采集光照与土壤湿度（SOIL_GetValue一次完成两路ADC转换）
		SOIL_GetValue();
		if( ldr_val >= ADC_MIN_VALID && ldr_val <= ADC_MAX_VALID &&
			soil_val >= ADC_MIN_VALID && soil_val <= ADC_MAX_VALID )
		{
			adc_ok = 1;
		}
		else
		{
			adc_ok = 0;
		}
		
		//2.3 拼接数据帧（MQTT与LoRa共用）T=25.6C,H=60.2%,LDR=1200,SOIL=3400
		memset(payload,0,sizeof(payload));
		sprintf(payload,"T=%.1fC,H=%.1f%%,LDR=%d,SOIL=%d",temp,humi,ldr_val,soil_val);
		printf("[DATA]%s\r\n",payload);
		
//		//2.4 MQTT上报到巴法云
//		if( mqtt_ok )
//		{
//			if( MQTT_Publish(MQTT_QoS0,payload,(char *)TOPIC) )
//			{
//				printf("[MQTT]publish ok\r\n");
//			}
//			else
//			{
//				printf("[MQTT]publish fail\r\n");
//				mqtt_ok = 0;	//标记掉线，交由重连逻辑处理
//			}
//		}
//		
//		//2.5 MQTT掉线重连（限制重连频率，避免长时间阻塞主循环）
//		if( !mqtt_ok && (HAL_GetTick() - lastMqttRetryTick >= MQTT_RETRY_MS) )
//		{
//			lastMqttRetryTick = HAL_GetTick();
//			printf("[MQTT]reconnect...\r\n");
//			if( ESP_Init() && MQTT_Connect((char *)USER_KEY) )
//			{
//				MQTT_Subscribe((char *)TOPIC,MQTT_QoS0);
//				mqtt_ok = 1;
//				printf("[MQTT]reconnect ok\r\n");
//			}
//		}
		
		//2.6 LoRa发送环境数据（上一包未发完则跳过本轮）
		if( LLCC68_SendPacket((uint8_t *)payload,strlen(payload)) )
		{
			printf("[LoRa]send:%s\r\n",payload);
		}
		
		//2.7 计算总状态并刷新OLED：成功/部分失败/失败
		AGRI_OledRefresh(temp,humi,ldr_val,soil_val,
		                 dht_ok,adc_ok,lora_ok,
		                 AGRI_CalcStatus(dht_ok,adc_ok));
	}
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
