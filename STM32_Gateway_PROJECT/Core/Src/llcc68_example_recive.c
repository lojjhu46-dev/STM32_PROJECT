#include "LLCC68_example_recive.h"
#include "radio.h"
#include "project_config.h"
#include "stdio.h"
#include "delay.h"
#include "string.h"

/*!
 * Radio events function pointer
 * 这个是传参进入其他函数中了，所以用全局变量(局部变量使用完了内存释放可能导致异常)
 */
static RadioEvents_t LLCC68RadioEvents;

/* 接收缓冲：必须在 RxDone 回调里立即拷贝
 * 原因：radio.c 的 RadioRxPayload[255] 是全局缓冲，下一包会直接覆盖 */
#define LORA_RX_BUF_SIZE	64
static uint8_t  rxBuf[LORA_RX_BUF_SIZE];
static uint8_t  rxLen  = 0;
static int16_t  rxRssi = 0;
static int8_t   rxSnr  = 0;
volatile uint8_t loraRxFlag = 0;	//1=收到新包待main取走

static uint32_t rxErrCount   = 0;	//CRC错误计数
static uint32_t rxTimeoutCnt = 0;	//接收超时计数(连续模式下不应发生)

static void LLCC68OnTxDone( void );
static void LLCC68OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr );
static void LLCC68OnTxTimeout( void );
static void LLCC68OnRxTimeout( void );
static void LLCC68OnRxError( void );

/* 接收示例：进入连续接收后轮询处理中断事件（死循环，仅供单独调试使用）
 * 业务代码请用 LLCC68_RecvInit + LLCC68_RecvIrqProcess + LLCC68_RecvGetPacket */
void ExampleLLCC68ReciveDemo(void)
{
	printf("start %s() example\r\n",__func__);
	
	LLCC68_RecvInit();
	
	while(1)
	{
		LLCC68_RecvIrqProcess();
		delay_ms(1);
	}
}

/* ================== 以下为非阻塞接口，供 main.c 业务主循环调用 ================== */

//LoRa接收初始化：注册事件回调 + 配置射频参数 + 进入连续接收（不进入死循环）
void LLCC68_RecvInit(void)
{
	printf("[LoRa]%s()\r\n",__func__);
	
	LLCC68RadioEvents.TxDone    = LLCC68OnTxDone;
	LLCC68RadioEvents.RxDone    = LLCC68OnRxDone;
	LLCC68RadioEvents.TxTimeout = LLCC68OnTxTimeout;
	LLCC68RadioEvents.RxTimeout = LLCC68OnRxTimeout;
	LLCC68RadioEvents.RxError   = LLCC68OnRxError;

	//注意：Radio.Init 内部会硬复位芯片并把 RadioEvents 指向本文件的 LLCC68RadioEvents，
	//若之后再调用 llcc68_example_send.c 的 LLCC68_RadioInit()，接收回调会被覆盖失效
	Radio.Init( &LLCC68RadioEvents );
	Radio.SetChannel( LORA_FRE );
	
	/* SetRxConfig 最后一个参数为 rxContinuous：
	 * true  = 连续接收，Radio.Rx(0) 后硬件持续监听，RxDone 之后无需重启接收
	 * false = 单次接收，每次 RxDone / RxTimeout 都要重新调用 Radio.Rx() */
	Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                     LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                     LORA_LLCC68_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                     0, true, 0, 0, LORA_IQ_INVERSION_ON, true );

	loraRxFlag = 0;
	rxLen      = 0;
	
	/* 进入连续接收：这里必须传 0
	 * 传非0会启动软件超时定时器(RadioRx内timeout!=0才启动)，
	 * 到点会误报 RxTimeout，反而打断连续接收 */
	Radio.Rx( 0 );
	
	printf("[LoRa]rx continuous start, freq=%d, SF=%d\r\n",LORA_FRE,LORA_SPREADING_FACTOR);
}

//主循环高频调用：处理DIO1中断事件（中断里只置 IrqFired 标志，真正的处理在这里）
void LLCC68_RecvIrqProcess(void)
{
	Radio.IrqProcess();
}

//取出一包数据：返回数据长度(字节)，0表示无新包；buf 内自动补 '\0' 结尾
uint8_t LLCC68_RecvGetPacket(char *buf, uint8_t bufLen, int16_t *rssi, int8_t *snr)
{
	uint8_t n;
	
	if( !loraRxFlag )	//没有新包
	{
		return 0;
	}
	
	loraRxFlag = 0;		//先清标志，允许回调写入下一包
	
	if( buf == NULL || bufLen == 0 )
	{
		return 0;
	}
	
	n = rxLen;
	if( n >= bufLen )	//预留1字节给 '\0'
	{
		n = bufLen - 1;
	}
	
	memcpy(buf, rxBuf, n);
	buf[n] = '\0';		//保证调用方可以安全使用 %s / strstr
	
	if( rssi != NULL )	*rssi = rxRssi;
	if( snr  != NULL )	*snr  = rxSnr;
	
	return n;
}

static void LLCC68OnTxDone( void )
{
	printf("[LoRa]TxDone\r\n");
}

static void LLCC68OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr )
{
	uint8_t n;
	
	//1.立即拷贝：RadioRxPayload 是 radio.c 的全局缓冲，下一包会覆盖
	n = ( size > (LORA_RX_BUF_SIZE - 1) ) ? (LORA_RX_BUF_SIZE - 1) : (uint8_t)size;
	memcpy(rxBuf, payload, n);
	rxBuf[n] = '\0';	//终端按 strlen 长度发送、不带结尾 '\0'，这里补上
	
	//2.记录链路质量与长度，仅置标志，耗时操作交给 main 处理
	rxLen  = n;
	rxRssi = rssi;
	rxSnr  = snr;
	loraRxFlag = 1;
	
	/* 3.连续接收模式下不要调用 Radio.Standby() / Radio.Rx()：
	 * RxContinuous==true 时 radio.c 不会把芯片切回 STDBY，硬件会持续监听；
	 * 在这里重启接收反而会打断正在进行的监听，造成丢包 */
}

static void LLCC68OnTxTimeout( void )
{
	printf("[LoRa]TxTimeout\r\n");
}

static void LLCC68OnRxTimeout( void )
{
	rxTimeoutCnt++;
	printf("[LoRa]RxTimeout(%u) re-enter rx\r\n",(unsigned int)rxTimeoutCnt);
	
	/* 连续接收模式下理论上不会触发（Radio.Rx(0) 没有启动软件超时定时器）
	 * 若确实发生，重新进入连续接收即可 */
	Radio.Rx( 0 );
}

static void LLCC68OnRxError( void )
{
	rxErrCount++;
	printf("[LoRa]RxError(%u), crc error\r\n",(unsigned int)rxErrCount);
	
	/* CRC 错误：连续接收模式下 radio.c 不会切换到 STDBY（RxContinuous==true 分支），
	 * 芯片仍处于监听状态，这里无需重启接收 */
}
