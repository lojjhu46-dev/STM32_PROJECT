#include "LLCC68_example_send.h"
#include "radio.h"
#include "project_config.h"
#include "stdio.h"
//#include "stm32f10x_it.h"
#include "delay.h"
#include "string.h"

/*!
 * Radio events function pointer
 * 这个是传参进入其他函数中了，所以用全局变量(局部变量使用完了内存释放可能导致异常)
 */
static RadioEvents_t LLCC68RadioEvents;

//LoRa发送完成标志：1=空闲可发送，0=正在发送中
//由 TxDone / TxTimeout 回调置1，发送前清0
volatile uint8_t loraTxDone = 1;

const char *sendData = "hello I am Test";

static void LLCC68OnTxDone( void );
static void LLCC68OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr );
static void LLCC68OnTxTimeout( void );
static void LLCC68OnRxTimeout( void );
static void LLCC68OnRxError( void );

static void LLCC68SendReconfig(void){
	LLCC68RadioEvents.TxDone = LLCC68OnTxDone;
	LLCC68RadioEvents.RxDone = LLCC68OnRxDone;
	LLCC68RadioEvents.TxTimeout = LLCC68OnTxTimeout;
	LLCC68RadioEvents.RxTimeout = LLCC68OnRxTimeout;
	LLCC68RadioEvents.RxError = LLCC68OnRxError;
	Radio.Init( &LLCC68RadioEvents );
	Radio.SetChannel(LORA_FRE);
	Radio.SetTxConfig( MODEM_LORA, LORA_TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                     LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                     LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                     true, 0, 0, LORA_IQ_INVERSION_ON, 3000 );//参数：lora模式,发射功率,fsk用的lora设置为0就可以，带宽，纠错编码率，前导码长度，固定长度数据包(一般是不固定的所以选false)，crc校验，0表示关闭跳频，跳频之间的符号数(关闭跳频这个参数没有意义)，这个应该是表示是否要翻转中断电平的，超时时间
	printf("LLCC68SendReconfig\r\n");
}

//开启一个定时发送任务，每隔1S发送一条数据
void ExampleLLCC68SendDemo(void){
	uint8_t OCP_Value = 0;
//	uint16_t count = 0;
	printf("start %s() example\r\n",__func__);
	
	LLCC68RadioEvents.TxDone = LLCC68OnTxDone;
	LLCC68RadioEvents.RxDone = LLCC68OnRxDone;
	LLCC68RadioEvents.TxTimeout = LLCC68OnTxTimeout;
	LLCC68RadioEvents.RxTimeout = LLCC68OnRxTimeout;
	LLCC68RadioEvents.RxError = LLCC68OnRxError;

	Radio.Init( &LLCC68RadioEvents );
	Radio.SetChannel(LORA_FRE);
	Radio.SetTxConfig( MODEM_LORA, LORA_TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                     LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                     LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                     true, 0, 0, LORA_IQ_INVERSION_ON, 3000 );//参数：lora模式,发射功率,fsk用的lora设置为0就可以，带宽，纠错编码率，前导码长度，固定长度数据包(一般是不固定的所以选false)，crc校验，0表示关闭跳频，跳频之间的符号数(关闭跳频这个参数没有意义)，这个应该是表示是否要翻转中断电平的，超时时间

	OCP_Value = Radio.Read(REG_OCP);
	printf("[%s()-%d]read OCP register value:0x%04X\r\n",__func__,__LINE__,OCP_Value);
	
	printf("all setting\r\n");
	printf("freq: %d\r\n Tx power: %d\r\n band width: %d\r\n FS: %d\r\n CODINGRATE: %d\r\n PREAMBLE_LENGTH: %d\r\n",LORA_FRE,LORA_TX_OUTPUT_POWER,LORA_BANDWIDTH,LORA_SPREADING_FACTOR,LORA_CODINGRATE,LORA_PREAMBLE_LENGTH);
	
	Radio.Send((uint8_t *)sendData,(strlen(sendData)+1));
	while(1){
		Radio.IrqProcess(); // Process Radio IRQ
		delay_ms(1);
	}
}

static void LLCC68OnTxDone( void )
{		
	printf("[LoRa]TXDone\r\n");
	Radio.Standby();
	loraTxDone = 1;		//释放总线，不再自动重发，由main每5秒触发一次发送
}

static void LLCC68OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr )
{
	Radio.Standby();
	printf("RxDone\r\nsize:%d\r\nrssi:%d\r\nsnr:%d\r\n",size,rssi,snr);
}

static void LLCC68OnTxTimeout( void )
{
	printf("[LoRa]TxTimeout\r\n");
	LLCC68SendReconfig();
	loraTxDone = 1;		//超时同样释放总线，避免主循环一直等不到发送完成
}

static void LLCC68OnRxTimeout( void )
{
	Radio.Standby();
	printf("RxTimeout retry recive\r\n");
	Radio.Rx( LORA_RX_TIMEOUT_VALUE ); 
}

static void LLCC68OnRxError( void )
{
	Radio.Standby();
	printf("RxError retry recive\r\n");
	Radio.Rx(LORA_RX_TIMEOUT_VALUE); 
}

/* ================== 以下为非阻塞接口，供 main.c 业务主循环调用 ================== */

//LoRa初始化：注册事件回调 + 配置射频参数（不进入死循环）
void LLCC68_RadioInit(void)
{
	printf("[LoRa]%s()\r\n",__func__);
	
	LLCC68RadioEvents.TxDone = LLCC68OnTxDone;
	LLCC68RadioEvents.RxDone = LLCC68OnRxDone;
	LLCC68RadioEvents.TxTimeout = LLCC68OnTxTimeout;
	LLCC68RadioEvents.RxTimeout = LLCC68OnRxTimeout;
	LLCC68RadioEvents.RxError = LLCC68OnRxError;

	Radio.Init( &LLCC68RadioEvents );
	Radio.SetChannel(LORA_FRE);
	Radio.SetTxConfig( MODEM_LORA, LORA_TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                     LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                     LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                     true, 0, 0, LORA_IQ_INVERSION_ON, 3000 );

	loraTxDone = 1;		//初始化完成，总线空闲
	printf("[LoRa]init done, freq=%d\r\n",LORA_FRE);
}

//发送一包数据：上一包未发完或长度非法返回0，成功启动发送返回1
uint8_t LLCC68_SendPacket(uint8_t *data, uint8_t len)
{
	if( !loraTxDone )	//上一包还在发送中，本次跳过
	{
		printf("[LoRa]busy, skip this packet\r\n");
		return 0;
	}
	
	if( data == NULL || len == 0 )
	{
		return 0;
	}
	
	loraTxDone = 0;
	Radio.Send((uint8_t *)data,len);
	return 1;
}

//主循环高频调用：处理DIO1中断事件（非阻塞）
void LLCC68_IrqProcess(void)
{
	Radio.IrqProcess();
}
