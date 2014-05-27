/*******************************************************************************
* �ļ���: Modbus.c                                                        
* ˵  ��: Modbus���������ļ�                                               
*******************************************************************************/
#include "Modbus.h"     //����Modbusͷ�ļ�
#include <stdlib.h>
#include <math.h>
#include "rtdef.h"


struct Modbusr_Configure MB_Configure=Modbus_Configure_Default;//Modbus�����������û�����
Modbus_RStruct MB_Read_Struct;                                  //Modbus�������������Ľṹ��(���ػ���)
Modbus_RRStruct MB_Read_RStruct;                                //Modbus���������صĽṹ��

//˽�к�������
static void RS485_TIM_Configure(struct RS485_Configure* Configure);
static void RS485_Send(uint8_t* buff,uint16_t len);
static void Modbus_Set_Timer(FunctionalState timer_state,uint8_t period_mode);
static uint8_t SetTIMPeriod(uint32_t period);
static void CRC_Check(uint8_t *data, uint16_t len, uint8_t *crc_data);


/*******************************************************************************
* ������ : Modbus_Init
* ����   : Modbus��ʼ��
* ����   : ��
* ����   : ��
*******************************************************************************/
int Modbus_INIT(void)
{
  RS485_TIM_Configure(&MB_Configure.Configure);
  MB_Configure=(struct Modbusr_Configure)Modbus_Configure_Default;//��MB_Configer�ָ�Ĭ������
  return 0;
}
INIT_COMPONENT_EXPORT(Modbus_INIT);


/*******************************************************************************
* ������ : Modbus_Read
* ����   : Modbus��ȡ����
* ����   : readstruct:ָ����������õĽṹ��ָ��
* ����   : �Ƿ��ȡ�ɹ�
*******************************************************************************/
Modbus_RRStruct* Modbus_Read(Modbus_RStruct* readstruct)
{
  uint8_t SendData[8]={0};
  uint8_t MB_ReceiveLen_Temp=0,MB_ReceiveLen=0,usart_rdata;
  uint8_t* pMB_ReceiveData=NULL;
  
  free(MB_Read_RStruct.ReceiveData);    //�ͷ��ϴν������ݿռ�
  MB_Read_RStruct.receive_len=0;        //����ϴν������ݳ���
  MB_Read_Struct.device_id=readstruct->device_id;//�������Ķ������ṹ�忽��������buffer
  MB_Read_Struct.function =readstruct->function;
  MB_Read_Struct.address  =readstruct->address;
  MB_Read_Struct.len      =readstruct->len;
  
  //���ô����͵����ݰ�
  SendData[0]=MB_Read_Struct.device_id;        SendData[1]=MB_Read_Struct.function;     //�����豸��ַ�Ͳ�����
  SendData[2]=(MB_Read_Struct.address>>8)&0xff;SendData[3]=MB_Read_Struct.address&0xff; //���ö�ȡ��ʼ��ַ
  SendData[4]=(MB_Read_Struct.len>>8)&0xff;    SendData[5]=MB_Read_Struct.len&0xff;     //���ö�д����
  
  CRC_Check(SendData,6,&SendData[6]);           //�Է������ݽ���CRCУ��
  MB_Configure.state=MB_SEND;                   //����Modbus����״̬Ϊ���ڷ���
  RS485_Send(SendData,8);                       //����RS485��������������
  MB_Configure.state=MB_WAITRESPONSE;           //����Modbus����״̬Ϊ�ȴ��ӻ���Ӧ
  Modbus_Set_Timer(ENABLE,MB_TIMER_WAIT);       //����Modbus��ʱ�����ȴ���վ����Ӧ
  
  while(1)
  {
    if(TIM_GetFlagStatus(TIM2, TIM_IT_Update) != RESET) //���TIM2�жϷ������
    {
      TIM_ClearITPendingBit(TIM2, TIM_IT_Update);       //���TIM2�жϱ�־
      if(MB_Configure.state==MB_WAITRESPONSE)           //Modbus����״̬Ϊ�ȴ��ӻ���Ӧ,���ʾ�ȴ��ӻ���Ӧ��ʱ��Modbusͨ�ŷ�������
      {
        Modbus_Set_Timer(DISABLE,NULL);
        MB_Configure.state=MB_FAILED;
        break;
      }
      else if(MB_Configure.state==MB_RECEIVE)           //Modbus����״̬Ϊ���ݽ���״̬��Modbusͨ��������
      {
        uint8_t crc_temp[2]={0};
        CRC_Check(pMB_ReceiveData,MB_ReceiveLen-2,crc_temp);
        if(MB_Configure.state==MB_FAILED || MB_ReceiveLen_Temp!=MB_ReceiveLen || crc_temp[0]!=pMB_ReceiveData[MB_ReceiveLen-2] || crc_temp[1]!=pMB_ReceiveData[MB_ReceiveLen-1])//���ղ���ȣ�����CRCУ�鲻��ȣ���������ݰ�ʧ��
        {
          Modbus_Set_Timer(DISABLE,NULL);
          MB_Configure.state=MB_FAILED;
          break;
        }
        else//���ճɹ�
        {
          Modbus_Set_Timer(DISABLE,NULL);
          MB_Configure.state=MB_OK;
          //�����������������ݶ������ؽṹ����
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
        usart_rdata=USART_ReceiveData(USART2);                  //��ȡusart���յ�������
        TIM2->CNT = 0;;                                         //��ʱ����ǰ��ʱֵ����
        if(MB_ReceiveLen_Temp==0)                               //�����յĵ�һ���ַ��Ƿ���ӵ�ַƥ��
        {
          if(usart_rdata==MB_Read_Struct.device_id)
          {
            MB_Configure.state=MB_RECEIVE;                        //����Modbus����״̬Ϊ���ڽ��մӻ���Ӧ
            Modbus_Set_Timer(ENABLE,MB_TIMER_RECEIVE);            //����Modbus��ʱ�����ȴ���վ����Ӧ
            MB_ReceiveLen_Temp++;
          }
          else
            MB_Configure.state=MB_FAILED;
        }
        else if(MB_ReceiveLen_Temp==1 && MB_Configure.state!=MB_FAILED)//���յĵڶ����ַ���������
        {
          if(usart_rdata==MB_Read_Struct.function)               //���������ƥ��
            MB_ReceiveLen_Temp++;
          else
            MB_Configure.state=MB_FAILED;
        }
        else if(MB_ReceiveLen_Temp==2 && MB_Configure.state!=MB_FAILED) //���յ������ַ������ݳ���
        {
          MB_ReceiveLen=usart_rdata+5;                          //����������ݵĳ���
          pMB_ReceiveData = (uint8_t*)malloc(MB_ReceiveLen);    //�����������ݻ�����
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
  free(pMB_ReceiveData);//�ͷŽ��չ���������Ĵ�Ž������ݵĿռ�
  return (&MB_Read_RStruct);
}



static void RS485_TIM_Configure(struct RS485_Configure* Configure)
{
  GPIO_InitTypeDef  GPIO_InitStructure;                 //����GPIO��ʼ���ṹ��
  USART_InitTypeDef USART_InitStructure;                //����USART��ʼ���ṹ��
  

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);//��USART2ʱ��
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);  //��TIM2ʱ��
  USART_DeInit(USART2);                                 //��λUSART2ģ��
  TIM_DeInit(TIM2);                                     //��λTIM2ģ��
  
  //USART2��GPIO����
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;             //USART2_TXʹ��PA2
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;             //USART2_RXʹ��PA3
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  
  
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;             //RS485�շ�ʹ��ʹ��F11
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOF, &GPIO_InitStructure);
  GPIO_ResetBits(GPIOF, GPIO_Pin_11);
  
  //USART2��������
  USART_InitStructure.USART_BaudRate = Configure->baud_rate;            //���ò�����
  if(Configure->parity == RS485_PARITY_EVEN || Configure->parity == RS485_PARITY_ODD)
    USART_InitStructure.USART_WordLength = USART_WordLength_9b;         //����żУ��λ����9λ���ݳ��� 
  else
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;         //����żУ��λ����8λ���ݳ���
  USART_InitStructure.USART_StopBits = USART_StopBits_1;                //1λֹͣλ
  USART_InitStructure.USART_Parity = Configure->parity;                 //������żУ��
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//��ֹӲ������(����ֹRTS��CTS)
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;       //ʹ�ܽ��պͷ���
  USART_Init(USART2, &USART_InitStructure);
  
  USART_Cmd(USART2, ENABLE);//ʹ��USART,������� 
}


static void RS485_Send(uint8_t* buff,uint16_t len)
{
  GPIO_SetBits(GPIOF, GPIO_Pin_11);     //��RS485�л�Ϊ����ģʽ
  USART_ClearFlag(USART2, USART_FLAG_TC);
  for(uint16_t i=0;i<len;i++)
  {
    USART_SendData(USART2,buff[i]);          //��������
    while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);//�ȴ����ͽ���
  }
  GPIO_ResetBits(GPIOF, GPIO_Pin_11);   //��RS485�л�Ϊ����ģʽ
}


static void Modbus_Set_Timer(FunctionalState timer_state,uint8_t period_mode)
{
  if(timer_state==ENABLE)       //��Modbus��ʱ��
  {
    if(period_mode==MB_TIMER_WAIT)
    {
      TIM_Cmd(TIM2,DISABLE);                                    //ֹͣModbus��ʱ��
      SetTIMPeriod((uint32_t)(1000*MB_Configure.wait_time));    //��վ�ȴ���վ��Ӧ��ʱ
      TIM_Cmd(TIM2,ENABLE);                                     //����Modbus��ʱ��
      TIM2->EGR |= TIM_PSCReloadMode_Immediate;                 //����TIM2��������¼�
      while(TIM_GetFlagStatus(TIM2, TIM_IT_Update) == RESET);   //���TIM2�жϷ������
      TIM_ClearITPendingBit(TIM2, TIM_IT_Update);               //���TIM2�жϱ�־
    }
    else
    {
      TIM_Cmd(TIM2,DISABLE);     //ֹͣModbus��ʱ��
      SetTIMPeriod((uint32_t)(35000000/MB_Configure.Configure.baud_rate));//��վ���ݽ��ս�����ʱ,��ʱ������Ϊ3.5���ֽڵ�ͨѶʱ��
      TIM_Cmd(TIM2,ENABLE);                                     //����Modbus��ʱ��
      TIM2->EGR |= TIM_PSCReloadMode_Immediate;                 //����TIM2��������¼�
      while(TIM_GetFlagStatus(TIM2, TIM_IT_Update) == RESET);  //���TIM2�жϷ������
      TIM_ClearITPendingBit(TIM2, TIM_IT_Update);               //���TIM2�жϱ�־
    }
  }
  else                  
    TIM_Cmd(TIM2,DISABLE);     //ֹͣModbus��ʱ��
}


static uint8_t SetTIMPeriod(uint32_t period)
{
  uint32_t Prescaler = 0;       //TIM�ķ�Ƶ��
  if(period == 0)               //�����ʱ����Ϊ0 ���򷵻�
    return 0;

  period = period*72;           //��������Ƶ�ʼ���TIME1������
  for(Prescaler=0;Prescaler<65536;Prescaler++)
  {
    if(!((period/(Prescaler+1))&0xFFFF0000))
      break;
  }
  
  if(Prescaler == 65536)         //ѡȡ������̫�����ʺϣ�����0
    return 0;
  
  period=(uint32_t)round((double)period/(double)(Prescaler+1))-1;

  TIM2->ARR = (uint16_t)(period & 0xFFFF);
  TIM2->PSC = Prescaler;
  
  return 1;                     //�������ڳɹ�������1
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