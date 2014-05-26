//-------------------------------------------------------------------------*
// 文件名: SPI.h                                                           *
// 说  明: SPI驱动头文件                                                   *
//-------------------------------------------------------------------------*

#ifndef __SPI_H__
#define __SPI_H__

//1 头文件
#include "stm32f10x.h"

//2 函数声明
//-------------------------------------------------------------------------*
//函数名: SPI_INIT                                                         *
//功  能: 初始化SPI模块                                                    *
//参  数: spich:SPI端口号(SPI1,SPI2,SPI3)                                  *
//返  回: 无                                                               *
//说  明:                                                                  *
//-------------------------------------------------------------------------*
void SPI_INIT(SPI_TypeDef* usartch);


//-------------------------------------------------------------------------*
//函数名: SPI_RW                                                           *
//功  能: SPI发送接收一个字节数据                                          *
//参  数: spich:SPI端口号(SPI1,SPI2,SPI3)                                  *
//        data:SPI待发送的数据                                             *
//返  回: SPI接收到的数据                                                  *
//说  明:                                                                  *
//-------------------------------------------------------------------------*
uint8_t SPI_RW(SPI_TypeDef* spich,uint8_t data);



#endif 
