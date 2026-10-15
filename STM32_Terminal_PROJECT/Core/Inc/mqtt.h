#define BUFFERSIZE	512	

//待链接的热点参数，注意：热点必须采用2.4GHZ
#define  	WIFI_SSID				"man"
#define  	WIFI_PASSWD			"33333344"

#define  	BEMFA_ADDR	 		"bemfa.com"	
#define 	BEMFA_TCP_PORT  8344
#define 	BEMFA_MQTT_PORT 9501

#define   TOPIC						"lFYDzLIsn004"
#define   USER_KEY				"741381524efe4b6a9056896f6faf3094"

#define   GET_BYTE0(len)		( *( (uint8_t *)(&len) + 0 ) )
#define   GET_BYTE1(len)		( *( (uint8_t *)(&len) + 1 ) )


#define   MQTT_QoS0  0
#define   MQTT_QoS1  1
#define   MQTT_QoS2  2


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#include "string.h"
#include "stdio.h"
#include "stdbool.h"
#include "usart.h"

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern uint8_t  wifi_rxtemp[BUFFERSIZE] ;  //用于接收单次空闲中断的数据
extern uint8_t  wifi_rxbuf[BUFFERSIZE] ;	//用于接收本次响应的完整数据
extern uint32_t wifi_rxlen ;									//用于记录接收数据的长度
extern uint8_t  wifi_rxflag ;

/* 函数声明（实现见 mqtt.c），供 main.c 调用 */
void  ESP_ExitPassThrough(void);										//退出透传
bool  ESP_SendCmd(char *cmd, char *deststr);				//发送AT指令并等待期望响应
bool  ESP_SendData(char *data, char *deststr);			//发送数据并等待期望响应
bool  ESP_Init(void);															//ESP8266初始化：连热点+连巴法云+进透传
bool  MQTT_Connect(char *clientid);									//MQTT连接报文，巴法云clientid填私钥USER_KEY
bool  MQTT_Subscribe(char *topic, uint8_t Qos);		//订阅主题
bool  MQTT_Publish(uint8_t Qos, char *buf, char *topic);	//发布消息

