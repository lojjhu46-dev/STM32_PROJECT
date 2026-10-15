#include "mqtt.h"
#include "main.h"

 uint8_t  wifi_rxtemp[BUFFERSIZE] ={0};  //用于接收单次空闲中断的数据
 uint8_t  wifi_rxbuf[BUFFERSIZE] ={0};	//用于接收本次响应的完整数据
 uint32_t wifi_rxlen = 0;									//用于记录接收数据的长度
 uint8_t  wifi_rxflag = 0;
//空闲中断的回调函数
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	if(huart->Instance == USART2)
	{
			if(wifi_rxlen+Size<BUFFERSIZE)
			{
				//拷贝单次接收数据
				memcpy(&wifi_rxbuf[wifi_rxlen],wifi_rxtemp,Size);
				wifi_rxlen += Size;
			}
			
			wifi_rxbuf[wifi_rxlen] = '\0'; //添加结束标志
			
			//清空临时缓冲区
			memset(wifi_rxtemp,0,sizeof(wifi_rxtemp));

			//启用UART2接收空闲中断
			HAL_UARTEx_ReceiveToIdle_IT(&huart2,wifi_rxtemp,BUFFERSIZE);  
	}	

}

//发送ESP命令
void  ESP_ExitPassThrough(void)
{
	//清空接收缓冲区
	memset(wifi_rxbuf,0,sizeof(wifi_rxbuf));
	wifi_rxlen = 0;
	HAL_Delay(1000);
	HAL_UART_Transmit(&huart2,(uint8_t *)"+++",strlen("+++"),1000);
	HAL_Delay(1000);
	
}

//发送ESP命令  成功 返回true  失败返回false
bool ESP_SendCmd(char *cmd,char *deststr)
{
	//清空接收缓冲区
	memset(wifi_rxbuf,0,sizeof(wifi_rxbuf));
	wifi_rxlen = 0;
	
	HAL_UART_Transmit(&huart2,(uint8_t *)cmd,strlen(cmd),1000);
	
	//增加超时机制 超时时间为10s
	int wait_cnt = 500;
	while(wait_cnt--)
	{
		if( strstr((char *)wifi_rxbuf,deststr) != NULL )
		{
			return true;
		}
		
		HAL_Delay(20);
	}
	
	return false;
}

//发送ESP数据  成功 返回true  失败返回false
bool ESP_SendData(char *data,char *deststr)
{
	//清空接收缓冲区
	memset(wifi_rxbuf,0,sizeof(wifi_rxbuf));
	wifi_rxlen = 0;
	
	HAL_UART_Transmit(&huart2,(uint8_t *)data,strlen(data),1000);
	
	//增加超时机制 超时时间为10s
	int wait_cnt = 500;
	while(wait_cnt--)
	{
		if( strstr((char *)wifi_rxbuf,deststr) != NULL )
		{
			return true;
		}
		
		HAL_Delay(20);
	}
	
	return false;
}

//ESP初始化
bool ESP_Init(void)
{
	bool status = true;
	
	printf("Esp Init...\r\n");
	
	//退出透传模式，只能在已经进入透传后使用
	ESP_ExitPassThrough();
	
	//发送AT测试指令
	if( ! ESP_SendCmd("AT\r\n","OK") )
	{
		status = false;
		printf("wifi online fail\r\n");
	}
		
	
	//设置WIFI模式
	if( ! ESP_SendCmd("AT+CWMODE=3\r\n","OK") )
	{
		status = false;
		printf("wifi mode fail\r\n");
	}
		

	//链接手机热点，一般会耗时较长
	char buf[BUFFERSIZE] = {0};
	sprintf(buf,"AT+CWJAP=\"%s\",\"%s\"\r\n",WIFI_SSID,WIFI_PASSWD);
	if ( ! ESP_SendCmd(buf,"OK"))
	{
		status = false;
		printf("wifi connect ap fail\r\n");
	}
		
	//开启透传模式，不意味着已经进入透传
	if( !ESP_SendCmd("AT+CIPMODE=1\r\n","OK"))
	{
		status = false;
		printf("wifi passthrough mode fail\r\n");
	}
	
	//链接服务器
	memset(buf,0,sizeof(buf));
	sprintf(buf,"AT+CIPSTART=\"TCP\",\"%s\",%d\r\n",BEMFA_ADDR,BEMFA_MQTT_PORT);
	if( !ESP_SendCmd(buf,"OK"))
	{
		status = false;
		printf("wifi connect cloud fail\r\n");
	}
	

	//进入透传
	if( !ESP_SendCmd("AT+CIPSEND\r\n",">"))
	{
		status = false;
		printf("wifi enter passthrough fail\r\n");
	}
		
	return status;
}


//连接报文
bool MQTT_Connect(char *clientid)
{
	static char msg[BUFFERSIZE] = {0}; 
	uint32_t cnt = 0;		//必须为局部变量，否则多次调用报文会累积错乱

	//计算剩余长度   剩余长度 = 可变报头 + 有效载荷(clientid utf-8)
	uint32_t remainder_len = 10 + strlen(clientid) + 2;
	
	
	//1.构造固定报头

	msg[cnt++] = 0x10;	//报文类型

	do
	{
		uint8_t encodedByte = remainder_len % 128;
		remainder_len = remainder_len / 128;
		
		// if there are more data to encode, set the top bit of this byte
		if ( remainder_len > 0 )
		{
			encodedByte = encodedByte | 128;
		}
		
		msg[cnt++] = encodedByte;
		
	}while ( remainder_len > 0 );
	
	//2.构造可变报头
	
	msg[cnt++] = 0;			//协议名称 MSB
	msg[cnt++] = 4;			//协议名称 LSB
	msg[cnt++] = 'M';		//协议名称
	msg[cnt++] = 'Q';		
	msg[cnt++] = 'T';		
	msg[cnt++] = 'T';		
	msg[cnt++] = 4;			//协议级别
	msg[cnt++] = 0x02;	//连接标志
	msg[cnt++] = 0;			//保活时间  MSB
	msg[cnt++] = 120;		//保活时间  LSB
	
	//3.构造有效载荷
	
	uint16_t clientid_len = strlen(clientid);
	
	msg[cnt++] = GET_BYTE1(clientid_len);			//客户端标识符  MSB
	msg[cnt++] = GET_BYTE0(clientid_len);			//客户端标识符  LSB
		
	memcpy(&msg[cnt],clientid,clientid_len);
	cnt += clientid_len;
	

	//4.清空接收缓冲区
	memset(wifi_rxbuf,0,sizeof(wifi_rxbuf));
	wifi_rxlen = 0;
	
	//5.发送连接报文
	HAL_UART_Transmit(&huart2,(uint8_t *)msg,cnt,1000);
	
	//6.等待服务器响应  增加超时机制 超时时间为5s
	int wait_cnt = 500;
	while(wait_cnt--)
	{
		if( wifi_rxbuf[0] == 0x20 && wifi_rxbuf[1] == 0x02 )
		{
			switch(wifi_rxbuf[3])
			{
				case 0x00: printf("连接已被服务端接受\n"); return true;
				case 0x01: printf("服务端不支持客户端请求的MQTT 协议级别\n"); return false;
				case 0x02: printf("客户端标识符是正确的UTF-8 编码，但服务端不允许使用\n"); return false;
				case 0x03: printf("网络连接已建立，但MQTT 服务不可用\n"); return false;
				case 0x04: printf("用户名或密码的数据格式无效\n"); return false;
				case 0x05: printf("客户端未被授权连接到此服务器\n"); return false;
			}
		}
		
		HAL_Delay(10);
	}
	
	return false;	//超时未收到服务器响应
}

//订阅主题
bool MQTT_Subscribe(char *topic,uint8_t Qos)
{
	static char msg[BUFFERSIZE] = {0}; 
	uint32_t cnt = 0;		//必须为局部变量，否则多次调用报文会累积错乱

	//计算剩余长度   剩余长度 = 可变报头(2) + 有效载荷(topic + 2 + 1)
	uint32_t remainder_len = 2 + strlen(topic) + 2 + 1;
	
	
	//1.构造固定报头

	msg[cnt++] = 0x82;	//报文类型

	do
	{
		uint8_t encodedByte = remainder_len % 128;
		remainder_len = remainder_len / 128;
		
		// if there are more data to encode, set the top bit of this byte
		if ( remainder_len > 0 )
		{
			encodedByte = encodedByte | 128;
		}
		
		msg[cnt++] = encodedByte;
		
	}while ( remainder_len > 0 );

	//2.构造可变报头
	
	msg[cnt++] = 0;									//报文标识符 MSB
	msg[cnt++] = strlen(topic);			//报文标识符 LSB
	
	//3.构造有效载荷
	
	uint16_t topic_len = strlen(topic);
	
	msg[cnt++] = GET_BYTE1(topic_len);			//主题名称长度  MSB
	msg[cnt++] = GET_BYTE0(topic_len);			//主题名称长度  LSB
		
	memcpy(&msg[cnt],topic,topic_len);
	cnt += topic_len;
	
	switch(Qos)															//服务质量  
	{
		case 0x00:msg[cnt++] = Qos; break;
		case 0x01:msg[cnt++] = Qos; break;
		case 0x02:msg[cnt++] = Qos; break;
		default : printf("服务质量异常，请检查Qos参数\n"); return false;
	}
	
	//4.清空接收缓冲区
	memset(wifi_rxbuf,0,sizeof(wifi_rxbuf));
	wifi_rxlen = 0;
	
	//5.发送连接报文
	HAL_UART_Transmit(&huart2,(uint8_t *)msg,cnt,1000);
	
	//6.等待服务器响应  增加超时机制 超时时间为5s
	int wait_cnt = 500;
	while(wait_cnt--)
	{
		if( wifi_rxbuf[0] == 0x90 && wifi_rxbuf[1] == 0x03 )
		{
			//判断返回码
			switch(wifi_rxbuf[4])
			{
				case 0x00: printf("订阅[%s]主题成功，最大QoS 0\r\n",topic); return true;
				case 0x01: printf("订阅[%s]主题成功，最大QoS 1\r\n",topic); return true;
				case 0x02: printf("订阅[%s]主题成功，最大QoS 2\r\n",topic); return true;
				case 0x80: printf("订阅[%s]主题失败\r\n",topic); return false;	
			}
		}
		
		HAL_Delay(10);
	}
	
	return false;	//超时未收到服务器响应
}

//发布消息
bool MQTT_Publish(uint8_t Qos,char *buf,char *topic)
{
	static char msg[BUFFERSIZE] = {0}; 
	uint32_t cnt = 0;		//必须为局部变量，否则多次调用报文会累积错乱
	uint32_t remainder_len = 0;
	
	//检查参数有效性  服务质量
	if(Qos != 0x00 && Qos != 0x01)
	{
		printf("Qos异常，请检查参数\r\n");
		return false;
	}
	
	//计算剩余长度   
	if(Qos == 0x00)
	{
		remainder_len = strlen(topic) + 2 + strlen(buf);
	}
	else
	{
		remainder_len = strlen(topic) + 2 + 2 + strlen(buf);
	}
	 
	//1.构造固定报头

	msg[cnt++] = 0x31 | (Qos<<1);	//报文类型

	do
	{
		uint8_t encodedByte = remainder_len % 128;
		remainder_len = remainder_len / 128;
		
		// if there are more data to encode, set the top bit of this byte
		if ( remainder_len > 0 )
		{
			encodedByte = encodedByte | 128;
		}
		
		msg[cnt++] = encodedByte;
		
	}while ( remainder_len > 0 );
	
	//2.构造可变报头

	uint16_t topic_len = strlen(topic);
	
	msg[cnt++] = GET_BYTE1(topic_len);			//主题名称长度  MSB
	msg[cnt++] = GET_BYTE0(topic_len);			//主题名称长度  LSB
		
	memcpy(&msg[cnt],topic,topic_len);
	cnt += topic_len;
	
	//Qos = 1的时候才需要报文标识符
	if(Qos)
	{
		msg[cnt++] = 0;									//报文标识符 MSB
		msg[cnt++] = strlen(topic);			//报文标识符 LSB
	}
	
	//3.构造有效载荷
	memcpy(&msg[cnt],buf,strlen(buf));
	cnt += strlen(buf);
	
	//4.清空接收缓冲区
	memset(wifi_rxbuf,0,sizeof(wifi_rxbuf));
	wifi_rxlen = 0;
	
	//5.发送连接报文
	HAL_UART_Transmit(&huart2,(uint8_t *)msg,cnt,1000);
	
	//6.只有Qos = 1才需要等待服务器响应  增加超时机制 超时时间为5s
	if( Qos )
	{
		int wait_cnt = 500;
		while(wait_cnt--)
		{
			if( wifi_rxbuf[0] == 0x40 && wifi_rxbuf[1] == 0x02 )
			{
				printf("消息[%s]发布到[%s]成功\r\n",buf,topic);
				return true;
			}
			
			HAL_Delay(10);
		}
		
		return false;
	}
	
	//Qos0 无确认机制，发送完成即认为成功
	return true;
}