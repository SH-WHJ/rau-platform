//-------------------------------------------------------------------------*
// �ļ���: SPI.c                                                           *
// ˵  ��: SPI���������ļ�                                                 *
//-------------------------------------------------------------------------*

#include "SPI.h"     //����SPIͷ�ļ�

//-------------------------------------------------------------------------*
//������: SPI_INIT                                                         *
//��  ��: ��ʼ��SPIģ��                                                    *
//��  ��: spich:SPI�˿ں�(SPI1,SPI2,SPI3)                                  *
//��  ��: ��                                                               *
//˵  ��:                                                                  *
//-------------------------------------------------------------------------*
void SPI_INIT(SPI_TypeDef* spich)
{
  GPIO_InitTypeDef  GPIO_InitStructure;                 //����GPIO��ʼ���ṹ��
  SPI_InitTypeDef SPI_InitStructure;                    //����SPI��ʼ���ṹ��
  SPI_I2S_DeInit(spich);                                //��λ����ʼ����SPIģ��

  if(spich == SPI1)
  {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE );          //��SPI1ʱ��
    //SPI1_SCKʹ��PA5;SPI1_MOSIʹ��PA7
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5  |  GPIO_Pin_7; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure); 
  
    //SPI1_MISOʹ��PA6
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure); 
  }
  else if(spich == SPI2)
  {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE );           //��SPI2ʱ��
    //SPI2_SCKʹ��PB13 ;SPI1_MOSIʹ��PB15
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13  |  GPIO_Pin_15; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure); 
  
    //SPI2_MISOʹ��PB14
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure); 
  }
  else if(spich == SPI3)
  {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE );          //��SPI3ʱ��
    //SPI3_SCKʹ��PB3; SPI3_MOSIʹ��PB5
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3  |  GPIO_Pin_5; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure); 
  
    //SPI3_MISOʹ��PB4
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure); 
  }
  else
    return;
  
  SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex; //˫��˫��ȫ˫��
  SPI_InitStructure.SPI_Mode = SPI_Mode_Master;         //����ģʽ
  SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;     //8λ����֡�ṹ 
  SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;            //ʱ�ӿ���ʱΪ��        
  SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;          //��1�������ز�������      
  SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;             //NSS�˿��������       
  SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8; //SPIʱ��(8��Ƶ) 72Mhz/8 = 9M
  SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;    //���ݴ����λ��ǰ 
  SPI_InitStructure.SPI_CRCPolynomial = 7;
  SPI_Init(spich, &SPI_InitStructure);//��ʼ��SPI
  SPI_Cmd(spich, ENABLE);//ʹ��SPI
}


//-------------------------------------------------------------------------*
//������: SPI_RW                                                           *
//��  ��: SPI���ͽ���һ���ֽ�����                                          *
//��  ��: spich:SPI�˿ں�(SPI1,SPI2,SPI3)                                  *
//        data:SPI�����͵�����                                             *
//��  ��: SPI���յ�������                                                  *
//˵  ��:                                                                  *
//-------------------------------------------------------------------------*
uint8_t SPI_RW(SPI_TypeDef* spich,uint8_t data)
{
  while (SPI_I2S_GetFlagStatus(spich, SPI_I2S_FLAG_TXE) == RESET);//�ȴ����ͻ���Ĵ���Ϊ��
  SPI_I2S_SendData(spich, data);//��������	
  while (SPI_I2S_GetFlagStatus(spich, SPI_I2S_FLAG_RXNE) == RESET);//�ȴ����ջ���Ĵ���Ϊ�ǿ�
  return SPI_I2S_ReceiveData(spich);
}