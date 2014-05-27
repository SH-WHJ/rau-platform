/*******************************************************************************
* 文件名: Modbus.h                                                       
* 说  明: Modbus驱动头文件                                              
*******************************************************************************/
#ifndef __Modbus_H__
#define __Modbus_H__

  //1 头文件
  #include "stm32f10x_gpio.h"

  //2 宏定义
  //Modbus数据类型宏定义(操作码)
  #define       Modbus_Coil                     0x01    //线圈(数字量输出)
  #define       Modbus_Contact                  0x02    //触点(数字量输入)
  #define       Modbus_HoldReg                  0x03    //保持寄存器(V存储区)
  #define       Modbus_InputReg                 0x04    //输入寄存器(模拟量输入)

  //2.3 Modbus定时器状态宏定义
  #define       MB_TIMER_WAIT                   0x0     //主站等待从站响应定时
  #define       MB_TIMER_RECEIVE                0x02    //主站数据接收结束定时

  #define       RS485_BAUD_RATE_4800            4800
  #define       RS485_BAUD_RATE_9600            9600
  #define       RS485_BAUD_RATE_38400           38400
  #define       RS485_BAUD_RATE_115200          115200
  
  #define       RS485_PARITY_NONE               USART_Parity_No
  #define       RS485_PARITY_ODD                USART_Parity_Odd
  #define       RS485_PARITY_EVEN               USART_Parity_Even



  #define       RS485_BAUD_RATE                 RS485_BAUD_RATE_115200
  #define       RS485_PARITY                    RS485_PARITY_NONE             
  #define       Modbus_Wait_Time                100//主站发送消息结束后等待从站响应的允许时间（ms）



  #define RS485_Configure_Default       \
  {                                     \
    RS485_BAUD_RATE,                    \
    RS485_PARITY_ODD                    \
  }
  
  #define Modbus_Configure_Default      \
  {                                     \
    RS485_Configure_Default,            \
    MB_IDLE,                            \
    Modbus_Wait_Time                    \
  }

  //3 数据类型定义
  enum MB_STATE {MB_IDLE=0,MB_OK,MB_FAILED,MB_SEND,MB_WAITRESPONSE,MB_RECEIVE};
  
  struct RS485_Configure
  {
    uint32_t baud_rate;
    uint16_t parity;
  };

  struct Modbusr_Configure
  {
    struct RS485_Configure Configure; //Modbus波特率
    enum MB_STATE state;   //Modbus通信状态
    uint16_t wait_time;    //主站发送消息结束后等待从站响应的允许时间（ms）
  };
  
  typedef struct 
  {
    uint8_t device_id;    //Modbus通信时的从机地址
    uint8_t function;     //读取的数据类型
    uint16_t address;     //读数据的起始地址(从0开始) 
    uint16_t len;         //所读数据的长度
  }Modbus_RStruct;
  
  
  typedef struct 
  {
    uint8_t *ReceiveData; //Modbus接收到的数据
    uint8_t receive_len;  //Modbus接收到的数据长度
  }Modbus_RRStruct;
  
  
/*******************************************************************************
* 函数名 : Modbus_Init
* 描述   : Modbus初始化
* 输入   : 无
* 返回   : 无
*******************************************************************************/
int Modbus_INIT(void);


/*******************************************************************************
* 函数名 : Modbus_Read
* 描述   : Modbus读取数据
* 输入   : readstruct:指向读操作配置的结构体指针
* 返回   : 是否读取成功
*******************************************************************************/
Modbus_RRStruct* Modbus_Read(Modbus_RStruct* readstruct);

#endif 