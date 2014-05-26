//-------------------------------------------------------------------------*
// 文件名: SPI.c                                                           *
// 说  明: SPI驱动程序文件                                                 *
//-------------------------------------------------------------------------*

#include "SPI.h"     //包含SPI头文件

//-------------------------------------------------------------------------*
//函数名: SPI_INIT                                                         *
//功  能: 初始化SPI模块                                                    *
//参  数: spich:SPI端口号(SPI1,SPI2,SPI3)                                  *
//返  回: 无                                                               *
//说  明:                                                                  *
//-------------------------------------------------------------------------*
void SPI_INIT(SPI_TypeDef* spich)
{
  GPIO_InitTypeDef  GPIO_InitStructure;                 //定义GPIO初始化结构体
  SPI_InitTypeDef SPI_InitStructure;                    //定义SPI初始化结构体
  SPI_I2S_DeInit(spich);                                //复位待初始化的SPI模块

  if(spich == SPI1)
  {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE );          //打开SPI1时钟
    //SPI1_SCK使用PA5;SPI1_MOSI使用PA7
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5  |  GPIO_Pin_7; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure); 
  
    //SPI1_MISO使用PA6
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure); 
  }
  else if(spich == SPI2)
  {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE );           //打开SPI2时钟
    //SPI2_SCK使用PB13 ;SPI1_MOSI使用PB15
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13  |  GPIO_Pin_15; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure); 
  
    //SPI2_MISO使用PB14
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure); 
  }
  else if(spich == SPI3)
  {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE );          //打开SPI3时钟
    //SPI3_SCK使用PB3; SPI3_MOSI使用PB5
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3  |  GPIO_Pin_5; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure); 
  
    //SPI3_MISO使用PB4
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure); 
  }
  else
    return;
  
  SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex; //双线双向全双工
  SPI_InitStructure.SPI_Mode = SPI_Mode_Master;         //主机模式
  SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;     //8位数据帧结构 
  SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;            //时钟空闲时为低        
  SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;          //第1个上升沿捕获数据      
  SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;             //NSS端口软件控制       
  SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8; //SPI时钟(8分频) 72Mhz/8 = 9M
  SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;    //数据传输高位在前 
  SPI_InitStructure.SPI_CRCPolynomial = 7;
  SPI_Init(spich, &SPI_InitStructure);//初始化SPI
  SPI_Cmd(spich, ENABLE);//使能SPI
}


//-------------------------------------------------------------------------*
//函数名: SPI_RW                                                           *
//功  能: SPI发送接收一个字节数据                                          *
//参  数: spich:SPI端口号(SPI1,SPI2,SPI3)                                  *
//        data:SPI待发送的数据                                             *
//返  回: SPI接收到的数据                                                  *
//说  明:                                                                  *
//-------------------------------------------------------------------------*
uint8_t SPI_RW(SPI_TypeDef* spich,uint8_t data)
{
  while (SPI_I2S_GetFlagStatus(spich, SPI_I2S_FLAG_TXE) == RESET);//等待发送缓冲寄存器为空
  SPI_I2S_SendData(spich, data);//发送数据	
  while (SPI_I2S_GetFlagStatus(spich, SPI_I2S_FLAG_RXNE) == RESET);//等待接收缓冲寄存器为非空
  return SPI_I2S_ReceiveData(spich);
}