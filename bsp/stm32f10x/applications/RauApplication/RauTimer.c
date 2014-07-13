/*******************************************************************************
* 文件名: Clock.c                                                        
* 说  明: Clock驱动程序文件                                               
*******************************************************************************/
#include "RauTimer.h"

LClock_t PastClock=0;    //TIM定时器溢出累加的Clock

/*******************************************************************************
* 函数名 : Clock_Init
* 描述   : Clock模块初始化
* 输入   : 无
* 返回   : 无
*******************************************************************************/
void Clock_Init(void)
{
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);  //打开TIM3时钟
  TIM_DeInit(TIM3);                                     //复位TIM3模块
  
  TIM_TimeBaseInitTypeDef  TIM_InitStructure;
  TIM_InitStructure.TIM_ClockDivision=TIM_CKD_DIV1;
  TIM_InitStructure.TIM_CounterMode=TIM_CounterMode_Up;
  TIM_InitStructure.TIM_Period=0xFFFF;
  TIM_InitStructure.TIM_Prescaler=35999;
  TIM_TimeBaseInit(TIM3,&TIM_InitStructure);
  
  TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
  
  NVIC_InitTypeDef NVIC_InitStructure; 
  NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn; 
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0; 
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0; 
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; 
  NVIC_Init(&NVIC_InitStructure);
  
  TIM_Cmd(TIM3, ENABLE);
  while(TIM_GetFlagStatus(TIM3, TIM_IT_Update) == RESET);   //检查TIM3中断发生与否
  TIM_ClearITPendingBit(TIM3, TIM_IT_Update);               //清除TIM3中断标志
}

/*******************************************************************************
* 函数名 : GetClock
* 描述   : 获取CLOCK时间
* 输入   : 无
* 返回   : 无
*******************************************************************************/
LClock_t GetClock(void)
{
  LClock_t clock;
  clock=PastClock+TIM3->CNT/2;
  return clock;
}

/*******************************************************************************
* 函数名 : DiffClock
* 描述   : 计算两次CLOCK时间差
* 输入   : 无
* 返回   : 无
*******************************************************************************/
LClock_t DiffClock(LClock_t StartClock)
{
  LClock_t StopClock=GetClock();
  return (StopClock-StartClock);
}

/*******************************************************************************
* 函数名 : TIM3_IRQHandler
* 描述   : TIM3中断响应函数
* 输入   : 无
* 返回   : 无
*******************************************************************************/
void TIM3_IRQHandler(void)
{
  if(TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET )    //检查TIM3更新中断发生与否
  {
    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);         //清除TIM3更新中断标志
    PastClock+=32763;
  }
} 