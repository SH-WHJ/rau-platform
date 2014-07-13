/*******************************************************************************
* �ļ���: Clock.c                                                        
* ˵  ��: Clock���������ļ�                                               
*******************************************************************************/
#include "RauTimer.h"

LClock_t PastClock=0;    //TIM��ʱ������ۼӵ�Clock

/*******************************************************************************
* ������ : Clock_Init
* ����   : Clockģ���ʼ��
* ����   : ��
* ����   : ��
*******************************************************************************/
void Clock_Init(void)
{
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);  //��TIM3ʱ��
  TIM_DeInit(TIM3);                                     //��λTIM3ģ��
  
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
  while(TIM_GetFlagStatus(TIM3, TIM_IT_Update) == RESET);   //���TIM3�жϷ������
  TIM_ClearITPendingBit(TIM3, TIM_IT_Update);               //���TIM3�жϱ�־
}

/*******************************************************************************
* ������ : GetClock
* ����   : ��ȡCLOCKʱ��
* ����   : ��
* ����   : ��
*******************************************************************************/
LClock_t GetClock(void)
{
  LClock_t clock;
  clock=PastClock+TIM3->CNT/2;
  return clock;
}

/*******************************************************************************
* ������ : DiffClock
* ����   : ��������CLOCKʱ���
* ����   : ��
* ����   : ��
*******************************************************************************/
LClock_t DiffClock(LClock_t StartClock)
{
  LClock_t StopClock=GetClock();
  return (StopClock-StartClock);
}

/*******************************************************************************
* ������ : TIM3_IRQHandler
* ����   : TIM3�ж���Ӧ����
* ����   : ��
* ����   : ��
*******************************************************************************/
void TIM3_IRQHandler(void)
{
  if(TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET )    //���TIM3�����жϷ������
  {
    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);         //���TIM3�����жϱ�־
    PastClock+=32763;
  }
} 