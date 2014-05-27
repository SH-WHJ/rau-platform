/*******************************************************************************
* �ļ���: Modbus.h                                                       
* ˵  ��: Modbus����ͷ�ļ�                                              
*******************************************************************************/
#ifndef __Modbus_H__
#define __Modbus_H__

  //1 ͷ�ļ�
  #include "stm32f10x_gpio.h"

  //2 �궨��
  //Modbus�������ͺ궨��(������)
  #define       Modbus_Coil                     0x01    //��Ȧ(���������)
  #define       Modbus_Contact                  0x02    //����(����������)
  #define       Modbus_HoldReg                  0x03    //���ּĴ���(V�洢��)
  #define       Modbus_InputReg                 0x04    //����Ĵ���(ģ��������)

  //2.3 Modbus��ʱ��״̬�궨��
  #define       MB_TIMER_WAIT                   0x0     //��վ�ȴ���վ��Ӧ��ʱ
  #define       MB_TIMER_RECEIVE                0x02    //��վ���ݽ��ս�����ʱ

  #define       RS485_BAUD_RATE_4800            4800
  #define       RS485_BAUD_RATE_9600            9600
  #define       RS485_BAUD_RATE_38400           38400
  #define       RS485_BAUD_RATE_115200          115200
  
  #define       RS485_PARITY_NONE               USART_Parity_No
  #define       RS485_PARITY_ODD                USART_Parity_Odd
  #define       RS485_PARITY_EVEN               USART_Parity_Even



  #define       RS485_BAUD_RATE                 RS485_BAUD_RATE_115200
  #define       RS485_PARITY                    RS485_PARITY_NONE             
  #define       Modbus_Wait_Time                100//��վ������Ϣ������ȴ���վ��Ӧ������ʱ�䣨ms��



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

  //3 �������Ͷ���
  enum MB_STATE {MB_IDLE=0,MB_OK,MB_FAILED,MB_SEND,MB_WAITRESPONSE,MB_RECEIVE};
  
  struct RS485_Configure
  {
    uint32_t baud_rate;
    uint16_t parity;
  };

  struct Modbusr_Configure
  {
    struct RS485_Configure Configure; //Modbus������
    enum MB_STATE state;   //Modbusͨ��״̬
    uint16_t wait_time;    //��վ������Ϣ������ȴ���վ��Ӧ������ʱ�䣨ms��
  };
  
  typedef struct 
  {
    uint8_t device_id;    //Modbusͨ��ʱ�Ĵӻ���ַ
    uint8_t function;     //��ȡ����������
    uint16_t address;     //�����ݵ���ʼ��ַ(��0��ʼ) 
    uint16_t len;         //�������ݵĳ���
  }Modbus_RStruct;
  
  
  typedef struct 
  {
    uint8_t *ReceiveData; //Modbus���յ�������
    uint8_t receive_len;  //Modbus���յ������ݳ���
  }Modbus_RRStruct;
  
  
/*******************************************************************************
* ������ : Modbus_Init
* ����   : Modbus��ʼ��
* ����   : ��
* ����   : ��
*******************************************************************************/
int Modbus_INIT(void);


/*******************************************************************************
* ������ : Modbus_Read
* ����   : Modbus��ȡ����
* ����   : readstruct:ָ����������õĽṹ��ָ��
* ����   : �Ƿ��ȡ�ɹ�
*******************************************************************************/
Modbus_RRStruct* Modbus_Read(Modbus_RStruct* readstruct);

#endif 