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
  * If no LICENSE file comes with this file, it is provided AS-IS.
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
#include "stdlib.h"
#include "string.h"
#include "mqtt.h"
#include "oled.h"
#include "LLCC68_example_recive.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define RX_LINK_TIMEOUT_MS	15000	//链路超时：超过15s未收到数据判为断链(终端5s一包，留3倍余量)
#define MQTT_RETRY_MS		30000	//MQTT掉线重连的最小间隔(ms)
#define LORA_RX_BUF_LEN		64		//接收帧缓冲(终端帧约35字节)
#define MQTT_BUF_LEN		128		//上报缓冲：原帧 + ",RSSI=-45,SNR=10"，余量充足
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
float    gwTemp = 0.0f;			//解析出的空气温度(℃)
float    gwHumi = 0.0f;			//解析出的空气湿度(%RH)
uint16_t gwLdr  = 0;			//解析出的光照
uint16_t gwSoil = 0;			//解析出的土壤湿度
int16_t  gwRssi = 0;			//最近一包的信号强度
int8_t   gwSnr  = 0;			//最近一包的信噪比
uint8_t  dataOk = 0;			//数据解析成功标志
uint8_t  mqttOk = 0;			//已连接巴法云MQTT标志
uint8_t  loraOk = 0;			//LoRa接收就绪标志
uint8_t  linkLost = 0;			//链路超时标志：超过15s没收到任何数据
uint32_t lastRxTick = 0;		//上次收到数据的时刻
uint32_t lastMqttRetryTick = 0;	//上次MQTT重连时刻
uint8_t  lastTotalSta = 0xFF;	//上次总状态，用于仅在状态变化时刷新OLED
char     rxFrame[LORA_RX_BUF_LEN];		//接收到的终端原始帧
char     mqttPayload[MQTT_BUF_LEN];		//MQTT上报内容
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

/* 解析终端数据帧：T=25.6C,H=60.2%,LDR=1200,SOIL=3400
   逐个字段定位解析，解析成功返回1，字段缺失返回0
   说明：不使用 sscanf 的 %f，避免部分运行库下 scanf 浮点转换不可用 */
static uint8_t GW_ParseFrame(char *frame)
{
	char *p;
	
	//T=25.6C
	p = strstr(frame,"T=");
	if( p == NULL )	return 0;
	gwTemp = (float)atof(p + 2);
	
	//H=60.2%
	p = strstr(frame,"H=");
	if( p == NULL )	return 0;
	gwHumi = (float)atof(p + 2);
	
	//LDR=1200
	p = strstr(frame,"LDR=");
	if( p == NULL )	return 0;
	gwLdr = (uint16_t)atoi(p + 4);
	
	//SOIL=3400
	p = strstr(frame,"SOIL=");
	if( p == NULL )	return 0;
	gwSoil = (uint16_t)atoi(p + 5);
	
	return 1;
}

/* 计算总状态：2=全部成功(ALL OK)  1=部分失败(PART FAIL)  0=失败(FAIL) */
static uint8_t GW_CalcStatus(void)
{
	if( linkLost || !dataOk )	return 0;	//链路超时或数据解析失败
	if( mqttOk && loraOk )		return 2;	//数据、MQTT、LoRa 全部正常
	return 1;								//数据正常但 MQTT 未连接
}

/* 刷新OLED：第1行温湿度、第2行光照与土壤、第3行模块状态、第4行总状态
   注意：每行最多容纳16个字符(128px/8px)，链路超时第1行显示 NO DATA */
static void GW_OledRefresh(uint8_t totalSta)
{
	char buf[17];
	
	OLED_Clear();
	
	//第1行：空气温湿度
	if( linkLost )
	{
		OLED_ShowString(0,0,(uint8_t *)"NO DATA",16);
	}
	else
	{
		if( dataOk ){ sprintf(buf,"T:%.1fC",gwTemp); }else{ sprintf(buf,"T:--"); }
		OLED_ShowString(0,0,(uint8_t *)buf,16);
		if( dataOk ){ sprintf(buf,"H:%.1f%%",gwHumi); }else{ sprintf(buf,"H:--"); }
		OLED_ShowString(64,0,(uint8_t *)buf,16);
	}
	
	//第2行：光照强度与土壤湿度
	if( dataOk ){ sprintf(buf,"L:%d",gwLdr); }else{ sprintf(buf,"L:--"); }
	OLED_ShowString(0,2,(uint8_t *)buf,16);
	if( dataOk ){ sprintf(buf,"S:%d",gwSoil); }else{ sprintf(buf,"S:--"); }
	OLED_ShowString(64,2,(uint8_t *)buf,16);
	
	//第3行：MQTT与LoRa模块状态
	OLED_ShowString(0,4,(uint8_t *)(mqttOk ? "MQ:OK" : "MQ:NG"),16);
	OLED_ShowString(64,4,(uint8_t *)(loraOk ? "LORA:OK" : "LORA:NG"),16);
	
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
	OLED_ShowString(0,0,(uint8_t *)"LORA GATEWAY",16);
	OLED_ShowString(0,2,(uint8_t *)"BOOTING...",16);
	printf("\r\n====== LORA GATEWAY START ======\r\n");
	
	//2.启动UART2空闲中断接收，必须在ESP_Init之前，否则收不到ESP8266响应
	HAL_UARTEx_ReceiveToIdle_IT(&huart2,wifi_rxtemp,BUFFERSIZE);
	
	//3.连接巴法云：ESP8266连热点与服务器 -> MQTT连接(clientid填私钥) -> 订阅主题
	//   注意：ESP_Init/MQTT_Connect内部是阻塞式AT等待，必须放在LoRa接收启动之前，
	//   否则连网期间主循环无法处理LoRa中断，会丢包
	OLED_ShowString(0,4,(uint8_t *)"WIFI CONNECT",16);
	if( ESP_Init() )
	{
		printf("[ESP]init ok\r\n");
		if( MQTT_Connect((char *)USER_KEY) )
		{
			printf("[MQTT]connect ok\r\n");
			if( MQTT_Subscribe((char *)TOPIC,MQTT_QoS0) )
			{
				printf("[MQTT]subscribe ok\r\n");
			}
			mqttOk = 1;
		}
		else
		{
			printf("[MQTT]connect fail\r\n");
		}
	}
	else
	{
		printf("[ESP]init fail\r\n");
	}
	
	//4.LoRa初始化并进入连续接收（网关只接收，不再调用发送侧的 LLCC68_RadioInit）
	OLED_ShowString(0,6,(uint8_t *)"LORA RX INIT",16);
	LLCC68_RecvInit();
	loraOk = 1;
	
	lastRxTick        = HAL_GetTick();
	lastMqttRetryTick = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
		
    /* USER CODE BEGIN 3 */
	uint8_t len;
	uint8_t totalSta;
	
	//1.高频处理LoRa中断事件，保证DIO1事件及时响应（非阻塞）
	LLCC68_RecvIrqProcess();
	
	//2.取一包接收到的数据
	len = LLCC68_RecvGetPacket(rxFrame,LORA_RX_BUF_LEN,&gwRssi,&gwSnr);
	if( len > 0 )
	{
		lastRxTick = HAL_GetTick();
		linkLost   = 0;
		
		printf("[LoRa]RX len=%d rssi=%d snr=%d\r\n",len,gwRssi,gwSnr);
		
		//2.1 解析温湿度、光照、土壤
		dataOk = GW_ParseFrame(rxFrame) ? 1 : 0;
		printf("[DATA]%s\r\n",rxFrame);
		
		//2.2 MQTT上报巴法云：终端原帧 + 链路质量 RSSI/SNR
		//    QoS0 不需要等待服务器应答，几乎不阻塞主循环，不会拖慢LoRa接收
		if( mqttOk )
		{
			sprintf(mqttPayload,"%s,RSSI=%d,SNR=%d",rxFrame,gwRssi,gwSnr);
			if( MQTT_Publish(MQTT_QoS0,mqttPayload,(char *)TOPIC) )
			{
				printf("[MQTT]publish ok\r\n");
			}
			else
			{
				printf("[MQTT]publish fail\r\n");
				mqttOk = 0;	//标记掉线，交由重连逻辑处理
			}
		}
	}
	
	//3.链路超时检测：超过15秒未收到任何数据，OLED显示NO DATA并置总状态为FAIL
	if( !linkLost && (HAL_GetTick() - lastRxTick >= RX_LINK_TIMEOUT_MS) )
	{
		linkLost = 1;
		dataOk   = 0;
		printf("[LoRa]link timeout, no data from terminal\r\n");
	}
	
	//4.MQTT掉线重连（限制重连频率，避免长时间阻塞主循环）
	if( !mqttOk && (HAL_GetTick() - lastMqttRetryTick >= MQTT_RETRY_MS) )
	{
		lastMqttRetryTick = HAL_GetTick();
		printf("[MQTT]reconnect...\r\n");
		if( ESP_Init() && MQTT_Connect((char *)USER_KEY) )
		{
			MQTT_Subscribe((char *)TOPIC,MQTT_QoS0);
			mqttOk = 1;
			printf("[MQTT]reconnect ok\r\n");
		}
	}
	
	//5.刷新OLED：收到新包或总状态变化时刷新，避免重复清屏造成闪烁
	totalSta = GW_CalcStatus();
	if( (len > 0) || (totalSta != lastTotalSta) )
	{
		GW_OledRefresh(totalSta);
		lastTotalSta = totalSta;
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
  * @brief  Reports the name of the source file and the location of
  *         where the assert_param error has occurred.
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
