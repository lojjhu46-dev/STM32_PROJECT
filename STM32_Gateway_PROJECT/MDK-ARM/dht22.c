   #include "dht22.h"
   #include "main.h" // 确保包含了 main.h 以获取 htim2 等定义
   
   /* 添加以下两行，告诉编译器这两个函数在其他 .c 文件中实现 */
   extern void delay_us(uint16_t nus);
   extern void delay_ms(uint16_t nms);

void DHT22_SetPinMode(uint32_t Mode)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

  /*Configure GPIO pin : DHT22_Pin_Pin */
  GPIO_InitStruct.Pin = DHT22_Pin;
  GPIO_InitStruct.Mode = Mode;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DHT22_GPIO_Port, &GPIO_InitStruct);
}


//开始信号
void DHT22_Start(void)
{
	//1.配置引脚为输出模式
	DHT22_SetPinMode(GPIO_MODE_OUTPUT_PP);
	
	//2.把引脚电平拉低，并持续至少1ms
	HAL_GPIO_WritePin(DHT22_GPIO_Port, DHT22_Pin, GPIO_PIN_RESET);
	delay_ms(1);
	
	//3.配置引脚为输入模式
	DHT22_SetPinMode(GPIO_MODE_INPUT);
}

//等待应答  
bool DHT22_WaitACK(void)
{
	uint32_t cnt = 0;
	
	//1.等待引脚电平被DHT22拉低  添加超时机制 假设为100us
	while( HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin) == 1 &&  cnt < 100)
	{
		cnt++;
		delay_us(1);
	}
	
	if(cnt>=100)
	{
		return false;
	}
	
	//2.检测低电平持续时间  添加超时机制 假设为100us
	cnt = 0;
	while( HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin) == 0 &&  cnt < 100)
	{
		cnt++;
		delay_us(1);
	}
	
	if(cnt>=100)
		return false;
	else
		return true;
}

//读取bit  0x00(0000000 0) 表示位数据0    0x01(0000000 1)  表示位数据1
uint8_t DHT22_ReadBit(void)
{
	uint32_t cnt = 0;
	
	//1.等待引脚电平被DHT22拉低  
	while( HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin) == 1 &&  cnt < 100)
	{
		cnt++;
		delay_us(1);
	}
	
	//2.等待引脚电平被DHT22拉高  
	cnt = 0;
	while( HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin) == 0 &&  cnt < 100)
	{
		cnt++;
		delay_us(1);
	}
	
	//3.延时一段时间 >28us && < 70us 并引脚电平
	delay_us(50);
	
	if(HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin))
		return 0x01;
	else
		return 0x00;
	
}

//读取字节
uint8_t DHT22_ReadByte(void)
{
	uint8_t data = 0; //用于记录读取的字节
	
	//循环读取8bit  MSB高位先出
	uint8_t cnt = 0;
	for(cnt=0;cnt<8;cnt++)
	{
		data <<= 1;
		data |= DHT22_ReadBit(); 	
	}
	
	return data;
}

bool DHT22_GetVal(float *temp,float *humi)
{
	static uint8_t buf[5] ={0};
	
	//1.发送开始信号
	DHT22_Start();
	
	//2.等待从机响应
	if( ! DHT22_WaitACK() )
	{
		printf("获取温湿度数据失败，DHT未应答\r\n");
		return false;
	}
	
	//3.获取完整数据  40bit
	int cnt =0;
	for(cnt=0;cnt<5;cnt++)
	{
		buf[cnt] = DHT22_ReadByte();
	}
	
	//4.对温湿度进行校验
	if(buf[0] + buf[1] + buf[2] + buf[3] != buf[4])
	{
		printf("获取温湿度数据失败，校验失败\r\n");
		return false;
	}
	
	//5.对数据进行处理
	uint16_t H = (buf[0] << 8) | buf[1];  //转换湿度
	uint16_t T = (buf[2] << 8) | buf[3];	//转换温度
	
	*humi = H / 10.0f;
	*temp = T / 10.0f;
	
	return true;
}