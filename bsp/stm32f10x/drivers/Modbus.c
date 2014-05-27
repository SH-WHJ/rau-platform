/*******************************************************************************
* 文件名: Modbus.c                                                        
* 说  明: Modbus驱动程序文件                                               
*******************************************************************************/
#include "Modbus.h"     //包含Modbus头文件
#include <stdlib.h>
#include <math.h>
#include "rtdef.h"


struct Modbusr_Configure MB_Configure=Modbus_Configure_Default;//Modbus基本参数配置机构体
Modbus_RStruct MB_Read_Struct;                                  //Modbus读操作传进来的结构体(本地缓存)
Modbus_RRStruct MB_Read_RStruct;                                //Modbus读操作返回的结构体

//私有函数申明
static void RS485_TIM_Configure(struct RS485_Configure* Configure);
static void RS485_Send(uint8_t* buff,uint16_t len);
static void Modbus_Set_Timer(FunctionalState timer_state,uint8_t period_mode);
static uint8_t SetTIMPeriod(uint32_t period);
static void CRC_Check(uint8_t *data, uint16_t len, uint8_t *crc_data);


/*******************************************************************************
* 函数名 : Modbus_Init
* 描述   : Modbus初始化
* 输入   : 无
* 返回   : 无
*******************************************************************************/
int Modbus_INIT(void)
{
  RS485_TIM_Configure(&MB_Configure.Configure);
  MB_Configure=(struct Modbusr_Configure)Modbus_Configure_Default;//将MB_Configer恢复默认设置
  return 0;
}
INIT_COMPONENT_EXPORT(Modbus_INIT);


/*******************************************************************************
* 函数名 : Modbus_Read
* 描述   : Modbus读取数据
* 输入   : readstruct:指向读操作配置的结构体指针
* 返回   : 是否读取成功
*******************************************************************************/
Modbus_RRStruct* Modbus_Read(Modbus_RStruct* readstruct)
{
  uint8_t SendData[8]={0};
  uint8_t MB_ReceiveLen_Temp=0,MB_ReceiveLen=0,usart_rdata;
  uint8_t* pMB_ReceiveData=NULL;
  
  free(MB_Read_RStruct.ReceiveData);    //释放上次接收数据空间
  MB_Read_RStruct.receive_len=0;        //清空上次接收数据长度
  MB_Read_Struct.device_id=readstruct->device_id;//将传进的读操作结构体拷贝至本地buffer
  MB_Read_Struct.function =readstruct->function;
  MB_Read_Struct.address  =readstruct->address;
  MB_Read_Struct.len      =readstruct->len;
  
  //配置代发送的数据包
  SendData[0]=MB_Read_Struct.device_id;        SendData[1]=MB_Read_Struct.function;     //配置设备地址和操作码
  SendData[2]=(MB_Read_Struct.address>>8)&0xff;SendData[3]=MB_Read_Struct.address&0xff; //配置读取起始地址
  SendData[4]=(MB_Read_Struct.len>>8)&0xff;    SendData[5]=MB_Read_Struct.len&0xff;     //配置读写长度
  
  CRC_Check(SendData,6,&SendData[6]);           //对发送数据进行CRC校验
  MB_Configure.state=MB_SEND;                   //设置Modbus工作状态为正在发送
  RS485_Send(SendData,8);                       //调用RS485驱动，发送数据
  MB_Configure.state=MB_WAITRESPONSE;           //设置Modbus工作状态为等待从机响应
  Modbus_Set_Timer(ENABLE,MB_TIMER_WAIT);       //启动Modbus定时器，等待从站的响应
  
  while(1)
  {
    if(TIM_GetFlagStatus(TIM2, TIM_IT_Update) != RESET) //检查TIM2中断发生与否
    {
      TIM_ClearITPendingBit(TIM2, TIM_IT_Update);       //清除TIM2中断标志
      if(MB_Configure.state==MB_WAITRESPONSE)           //Modbus工作状态为等待从机响应,则表示等待从机响应超时，Modbus通信发生错误
      {
        Modbus_Set_Timer(DISABLE,NULL);
        MB_Configure.state=MB_FAILED;
        break;
      }
      else if(MB_Configure.state==MB_RECEIVE)           //Modbus工作状态为数据接收状态，Modbus通信正常，
      {
        uint8_t crc_temp[2]={0};
        CRC_Check(pMB_ReceiveData,MB_ReceiveLen-2,crc_temp);
        if(MB_Configure.state==MB_FAILED || MB_ReceiveLen_Temp!=MB_ReceiveLen || crc_temp[0]!=pMB_ReceiveData[MB_ReceiveLen-2] || crc_temp[1]!=pMB_ReceiveData[MB_ReceiveLen-1])//接收不相等，并且CRC校验不相等，则接收数据包失败
        {
          Modbus_Set_Timer(DISABLE,NULL);
          MB_Configure.state=MB_FAILED;
          break;
        }
        else//接收成功
        {
          Modbus_Set_Timer(DISABLE,NULL);
          MB_Configure.state=MB_OK;
          //拷贝接收数据中数据段至返回结构体中
          MB_Read_RStruct.receive_len=MB_ReceiveLen-5;
          MB_Read_RStruct.ReceiveData=(uint8_t*)malloc(MB_Read_RStruct.receive_len);
          for(uint16_t i=3;i<MB_ReceiveLen-2;i++)
            MB_Read_RStruct.ReceiveData[i-3]=pMB_ReceiveData[i];
          break;
        }
      }
    }
    
    if(USART_GetFlagStatus(USART2,USART_FLAG_RXNE) == SET)
    {
        usart_rdata=USART_ReceiveData(USART2);                  //获取usart接收到的数据
        TIM2->CNT = 0;;                                         //定时器当前定时值清零
        if(MB_ReceiveLen_Temp==0)                               //检测接收的第一个字符是否与从地址匹配
        {
          if(usart_rdata==MB_Read_Struct.device_id)
          {
            MB_Configure.state=MB_RECEIVE;                        //设置Modbus工作状态为正在接收从机响应
            Modbus_Set_Timer(ENABLE,MB_TIMER_RECEIVE);            //启动Modbus定时器，等待从站的响应
            MB_ReceiveLen_Temp++;
          }
          else
            MB_Configure.state=MB_FAILED;
        }
        else if(MB_ReceiveLen_Temp==1 && MB_Configure.state!=MB_FAILED)//接收的第二个字符，操作码
        {
          if(usart_rdata==MB_Read_Struct.function)               //如果操作码匹配
            MB_ReceiveLen_Temp++;
          else
            MB_Configure.state=MB_FAILED;
        }
        else if(MB_ReceiveLen_Temp==2 && MB_Configure.state!=MB_FAILED) //接收第三个字符，数据长度
        {
          MB_ReceiveLen=usart_rdata+5;                          //计算接收数据的长度
          pMB_ReceiveData = (uint8_t*)malloc(MB_ReceiveLen);    //创建发送数据缓冲区
          pMB_ReceiveData[0]=MB_Read_Struct.device_id; pMB_ReceiveData[1]=MB_Read_Struct.function; pMB_ReceiveData[2]=usart_rdata;
          MB_ReceiveLen_Temp++;
        }
        else if(MB_ReceiveLen_Temp<MB_ReceiveLen && MB_Configure.state!=MB_FAILED)
        {
          pMB_ReceiveData[MB_ReceiveLen_Temp]=usart_rdata;
          MB_ReceiveLen_Temp++;
        }
        else if(MB_Configure.state!=MB_FAILED)
          MB_Configure.state=MB_FAILED;
    }
  }
  free(pMB_ReceiveData);//释放接收过程中申请的存放接收数据的空间
  return (&MB_Read_RStruct);
}



static void RS485_TIM_Configure(struct RS485_Configure* Configure)
{
  GPIO_InitTypeDef  GPIO_InitStructure;                 //定义GPIO初始化结构体
  USART_InitTypeDef USART_InitStructure;                //定义USART初始化结构体
  

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);//打开USART2时钟
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);  //打开TIM2时钟
  USART_DeInit(USART2);                                 //复位USART2模块
  TIM_DeInit(TIM2);                                     //复位TIM2模块
  
  //USART2的GPIO配置
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;             //USART2_TX使用PA2
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;             //USART2_RX使用PA3
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  
  
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;             //RS485收发使能使用F11
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOF, &GPIO_InitStructure);
  GPIO_ResetBits(GPIOF, GPIO_Pin_11);
  
  //USART2基本配置
  USART_InitStructure.USART_BaudRate = Configure->baud_rate;            //设置波特率
  if(Configure->parity == RS485_PARITY_EVEN || Configure->parity == RS485_PARITY_ODD)
    USART_InitStructure.USART_WordLength = USART_WordLength_9b;         //有奇偶校验位，则9位数据长度 
  else
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;         //无奇偶校验位，则8位数据长度
  USART_InitStructure.USART_StopBits = USART_StopBits_1;                //1位停止位
  USART_InitStructure.USART_Parity = Configure->parity;                 //设置奇偶校验
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//禁止硬件流控(即禁止RTS和CTS)
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;       //使能接收和发送
  USART_Init(USART2, &USART_InitStructure);
  
  USART_Cmd(USART2, ENABLE);//使能USART,配置完毕 
}


static void RS485_Send(uint8_t* buff,uint16_t len)
{
  GPIO_SetBits(GPIOF, GPIO_Pin_11);     //将RS485切换为发送模式
  USART_ClearFlag(USART2, USART_FLAG_TC);
  for(uint16_t i=0;i<len;i++)
  {
    USART_SendData(USART2,buff[i]);          //发送数据
    while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);//等待发送结束
  }
  GPIO_ResetBits(GPIOF, GPIO_Pin_11);   //将RS485切换为接收模式
}


static void Modbus_Set_Timer(FunctionalState timer_state,uint8_t period_mode)
{
  if(timer_state==ENABLE)       //打开Modbus定时器
  {
    if(period_mode==MB_TIMER_WAIT)
    {
      TIM_Cmd(TIM2,DISABLE);                                    //停止Modbus定时器
      SetTIMPeriod((uint32_t)(1000*MB_Configure.wait_time));    //主站等待从站响应定时
      TIM_Cmd(TIM2,ENABLE);                                     //启动Modbus定时器
      TIM2->EGR |= TIM_PSCReloadMode_Immediate;                 //启动TIM2软件更新事件
      while(TIM_GetFlagStatus(TIM2, TIM_IT_Update) == RESET);   //检查TIM2中断发生与否
      TIM_ClearITPendingBit(TIM2, TIM_IT_Update);               //清除TIM2中断标志
    }
    else
    {
      TIM_Cmd(TIM2,DISABLE);     //停止Modbus定时器
      SetTIMPeriod((uint32_t)(35000000/MB_Configure.Configure.baud_rate));//主站数据接收结束定时,定时器周期为3.5个字节的通讯时间
      TIM_Cmd(TIM2,ENABLE);                                     //启动Modbus定时器
      TIM2->EGR |= TIM_PSCReloadMode_Immediate;                 //启动TIM2软件更新事件
      while(TIM_GetFlagStatus(TIM2, TIM_IT_Update) == RESET);  //检查TIM2中断发生与否
      TIM_ClearITPendingBit(TIM2, TIM_IT_Update);               //清除TIM2中断标志
    }
  }
  else                  
    TIM_Cmd(TIM2,DISABLE);     //停止Modbus定时器
}


static uint8_t SetTIMPeriod(uint32_t period)
{
  uint32_t Prescaler = 0;       //TIM的分频数
  if(period == 0)               //如果定时周期为0 ，则返回
    return 0;

  period = period*72;           //根据总线频率计算TIME1的周期
  for(Prescaler=0;Prescaler<65536;Prescaler++)
  {
    if(!((period/(Prescaler+1))&0xFFFF0000))
      break;
  }
  
  if(Prescaler == 65536)         //选取的周期太长不适合，返回0
    return 0;
  
  period=(uint32_t)round((double)period/(double)(Prescaler+1))-1;

  TIM2->ARR = (uint16_t)(period & 0xFFFF);
  TIM2->PSC = Prescaler;
  
  return 1;                     //设置周期成功，返回1
}


static void CRC_Check(uint8_t *data, uint16_t len, uint8_t *crc_data) 
{ 
  uint16_t crc_temp,crc=0xFFFF; 
  for(uint16_t i = 0; i < len; i ++) 
  { 
    crc_temp = data[i] & 0x00FF; 
    crc ^= crc_temp; 
    for(uint8_t j = 0;j < 8; j ++) 
    {  
      if (crc & 0x0001) 
      { 
        crc>>=1; 
        crc^=0xA001; 
      } 
      else crc>>=1; 
    } 
  }
  crc_data[0]=crc&0xff;
  crc_data[1]=(crc>>8)&0xff;
}