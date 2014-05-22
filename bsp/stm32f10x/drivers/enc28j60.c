/*
 * File      : enc28j60.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2009, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2009-05-05     Bernard      the first version
 */
#include "enc28j60.h"
#include "SPI.h"
#include <netif/ethernetif.h>
#include <stm32f10x.h>
#include <stm32f10x_spi.h>
#include <rtdef.h>

#define MAX_ADDR_LEN    6

//ENC28J60��CS����ʹ��PTA4
#define ENC28J60_SPI    SPI1                    //ENC28J60ʹ��SPI1ģ��        
#define CSACTIVE 	GPIOA->BRR = GPIO_Pin_4; //ENC28J60_CS����
#define CSPASSIVE	GPIOA->BSRR = GPIO_Pin_4;//ENC28J60_CS����

struct net_device
{
  /* inherit from ethernet device */
  struct eth_device parent;

  rt_uint8_t  dev_addr[MAX_ADDR_LEN];//MAC��ַ
};


static struct net_device  enc28j60_dev_entry;                   //����һ�������豸����
static struct net_device *enc28j60_dev =&enc28j60_dev_entry;    //����ָ�������豸�����ָ��
static rt_uint16_t NextPacketPtr;                               //�洢��һ�����ݰ����׵�ַ
static struct rt_semaphore lock_sem;                            //����enc28j60���ź�������

//˽�к�������
static rt_err_t enc28j60_init(rt_device_t dev);
static struct pbuf *enc28j60_rx(rt_device_t dev);
static rt_err_t enc28j60_tx( rt_device_t dev, struct pbuf* p);
static rt_err_t enc28j60_open(rt_device_t dev, rt_uint16_t oflag);
static rt_err_t enc28j60_close(rt_device_t dev);
static rt_size_t enc28j60_read(rt_device_t dev, rt_off_t pos, void* buffer, rt_size_t size);
static rt_size_t enc28j60_write(rt_device_t dev, rt_off_t pos, const void* buffer, rt_size_t size);
static rt_err_t enc28j60_control(rt_device_t dev, rt_uint8_t cmd, void *args);
rt_inline rt_uint8_t enc28j60_interrupt_disable();
rt_inline void enc28j60_interrupt_enable(rt_uint8_t level);
static void enc28j60_writeop(rt_uint8_t op, rt_uint8_t address, rt_uint8_t data);
static rt_uint8_t enc28j60_readop(rt_uint8_t op, rt_uint8_t address);
static void enc28j60_setbank(rt_uint8_t address);
static rt_uint8_t enc28j60_readreg(rt_uint8_t address);
static void enc28j60_writereg(rt_uint8_t address, rt_uint8_t data);
static rt_uint16_t enc28j60_readphy(rt_uint8_t address);
static void enc28j60_writephy(rt_uint8_t address, rt_uint16_t data);
void enc28j60_clkout(rt_uint8_t clk);
static void GPIO_Configure();
static rt_bool_t enc28j60_check_link_status();

int rt_hw_enc28j60_init()
{
  GPIO_Configure();
  SPI_INIT(ENC28J60_SPI);                                 //SPI��ʼ��

  /* init rt-thread device interface */
  enc28j60_dev_entry.parent.parent.init		= enc28j60_init;
  enc28j60_dev_entry.parent.parent.open		= enc28j60_open;
  enc28j60_dev_entry.parent.parent.close	= enc28j60_close;
  enc28j60_dev_entry.parent.parent.read		= enc28j60_read;
  enc28j60_dev_entry.parent.parent.write	= enc28j60_write;
  enc28j60_dev_entry.parent.parent.control	= enc28j60_control;
  enc28j60_dev_entry.parent.eth_rx	        = enc28j60_rx;
  enc28j60_dev_entry.parent.eth_tx	        = enc28j60_tx;

  //{0x32,0x12,0x35,0x11,0x01,0x51};  //MAC~{5XV7~}
  enc28j60_dev_entry.dev_addr[0] = 0x00;
  enc28j60_dev_entry.dev_addr[1] = 0x22;
  enc28j60_dev_entry.dev_addr[2] = 0x6c;
  enc28j60_dev_entry.dev_addr[3] = 0x11;
  enc28j60_dev_entry.dev_addr[4] = 0x56;
  enc28j60_dev_entry.dev_addr[5] = 0x92;

  rt_sem_init(&lock_sem, "lock", 1, RT_IPC_FLAG_FIFO);//����һ����̬�ź���
  eth_device_init(&(enc28j60_dev->parent), "e0");
  
  return 0;
}


/*
 * RX handler
 * ignore PKTIF because is unreliable! (look at the errata datasheet)
 * check EPKTCNT is the suggested workaround.
 * We don't need to clear interrupt flag, automatically done when
 * enc28j60_hw_rx() decrements the packet counter.
 */
void enc28j60_isr()
{
  rt_uint8_t eir, pk_counter;
  rt_bool_t rx_activiated;
  eir = enc28j60_readreg(EIR);//��ȡenc28j60�ж������־λ
  do
  {
    pk_counter = enc28j60_readreg(EPKTCNT);//��ȡenc28j60�����������ݰ��ĸ���
    if (pk_counter)
    {
      eth_device_ready((struct eth_device*)&(enc28j60_dev->parent));//�����ʼ�����ʾ����֡���յ�
      enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, EIE, EIE_PKTIE);//��ֹ�����ж�
    }
    if (eir & EIR_PKTIF)//���յ����ݰ�
    {
      rx_activiated = RT_TRUE;
    }
    if ((eir & EIR_LINKIF) != 0)//����״̬�ı��ж�
    {
      rt_bool_t up=enc28j60_check_link_status();
      eth_device_linkchange((struct eth_device*)&(enc28j60_dev->parent), up);
      enc28j60_readphy(PHIR);//��ȡPHIR�Ĵ��� �������״̬�ı��жϱ�־λ
    }
    
    
    if (eir & EIR_TXIF)//�����ж�
    {
      enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, EIR, EIR_TXIF);//��������жϱ�־λ
    }
    if ((eir & EIR_TXERIF) != 0)//���ʹ����ж�
    {
      enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, EIR, EIR_TXERIF);//������ʹ����жϱ�־λ
    }
    if ((eir & EIR_RXERIF) != 0)//���մ����ж�
    {
      enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, EIR, EIR_RXERIF);//������մ����жϱ�־λ
    }
    if ((eir & EIR_DMAIF) != 0)//DMA�ж�
    {
      enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, EIR, EIR_DMAIF);//���DMA�ϱ�־λ
    }
    if ((eir & EIR_WOLIF) != 0)//WOL�ж�
    {
      enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, EIR, EIR_WOLIF);//���WOL�жϱ�־λ
    }
    eir = enc28j60_readreg(EIR);//��ȡenc28j60�ж������־λ
  }while (rx_activiated != RT_TRUE && eir != 0);
}


#ifdef RT_USING_FINSH
/*
 * Debug routine to dump useful register contents
 */
static void enc28j60(void)
{
  rt_kprintf("-- enc28j60 registers:\n");
  rt_kprintf("HwRevID: 0x%02x\n", enc28j60_readreg(EREVID));
  rt_kprintf("Cntrl: ECON1 ECON2 ESTAT  EIR  EIE\n");
  rt_kprintf("       0x%02x  0x%02x  0x%02x  0x%02x  0x%02x\n",enc28j60_readreg(ECON1), enc28j60_readreg(ECON2), enc28j60_readreg(ESTAT), enc28j60_readreg(EIR), enc28j60_readreg(EIE));
  rt_kprintf("MAC  : MACON1 MACON3 MACON4\n");
  rt_kprintf("       0x%02x   0x%02x   0x%02x\n", enc28j60_readreg(MACON1), enc28j60_readreg(MACON3), enc28j60_readreg(MACON4));
  rt_kprintf("Rx   : ERXST  ERXND  ERXWRPT ERXRDPT ERXFCON EPKTCNT MAMXFL\n");
  rt_kprintf("       0x%04x 0x%04x 0x%04x  0x%04x  ",
          (enc28j60_readreg(ERXSTH) << 8) | enc28j60_readreg(ERXSTL),
          (enc28j60_readreg(ERXNDH) << 8) | enc28j60_readreg(ERXNDL),
          (enc28j60_readreg(ERXWRPTH) << 8) | enc28j60_readreg(ERXWRPTL),
          (enc28j60_readreg(ERXRDPTH) << 8) | enc28j60_readreg(ERXRDPTL));
  rt_kprintf("0x%02x    0x%02x    0x%04x\n", enc28j60_readreg(ERXFCON), enc28j60_readreg(EPKTCNT),
          (enc28j60_readreg(MAMXFLH) << 8) | enc28j60_readreg(MAMXFLL));

  rt_kprintf("Tx   : ETXST  ETXND  MACLCON1 MACLCON2 MAPHSUP\n");
  rt_kprintf("       0x%04x 0x%04x 0x%02x     0x%02x     0x%02x\n",
          (enc28j60_readreg(ETXSTH) << 8) | enc28j60_readreg(ETXSTL),
          (enc28j60_readreg(ETXNDH) << 8) | enc28j60_readreg(ETXNDL),
          enc28j60_readreg(MACLCON1), enc28j60_readreg(MACLCON2), enc28j60_readreg(MAPHSUP));
}
#include <finsh.h>
FINSH_FUNCTION_EXPORT(enc28j60, dump enc28j60 registers);
#endif




static rt_err_t enc28j60_init(rt_device_t dev)
{
  enc28j60_writeop(ENC28J60_SOFT_RESET,0,ENC28J60_SOFT_RESET);//ϵͳ��λ
  while(!(enc28j60_readreg(ESTAT) & ESTAT_CLKRDY));//��ѯESTAT.CLKRDYλ,�ȴ�ʱ�Ӿ���

  //���ý��ջ�������ʼ��ַ
  NextPacketPtr = RXSTART_INIT;
  enc28j60_writereg(ERXSTL, RXSTART_INIT&0xFF);
  enc28j60_writereg(ERXSTH, RXSTART_INIT>>8);
  //���ý�ֹӲ������д��FIFO�ĵ�ַ
  enc28j60_writereg(ERXRDPTL, RXSTOP_INIT&0xFF);
  enc28j60_writereg(ERXRDPTH, RXSTOP_INIT>>8);
  //���ý��ջ�����������ַ
  enc28j60_writereg(ERXNDL, RXSTOP_INIT&0xFF);
  enc28j60_writereg(ERXNDH, RXSTOP_INIT>>8);

  //���÷��ͻ�������ʼ��ַ
  enc28j60_writereg(ETXSTL, TXSTART_INIT&0xFF);
  enc28j60_writereg(ETXSTH, TXSTART_INIT>>8);
  //���÷��ͻ�����дָ��
  enc28j60_writereg(EWRPTL, TXSTART_INIT&0xFF);
  enc28j60_writereg(EWRPTH, TXSTART_INIT>>8);
  //���÷��ͻ�����������ַ
  enc28j60_writereg(ETXNDL, TXSTOP_INIT&0xFF);
  enc28j60_writereg(ETXNDH, TXSTOP_INIT>>8);

//ʹ�ܵ������� ʹ��CRCУ�� ʹ�� ��ʽƥ���Զ����� 
  enc28j60_writereg(ERXFCON, ERXFCON_UCEN|ERXFCON_CRCEN|ERXFCON_BCEN);
  /*enc28j60_writereg(ERXFCON, ERXFCON_UCEN|ERXFCON_CRCEN|ERXFCON_PMEN);  
  enc28j60_writereg(EPMM0, 0x3f);  
  enc28j60_writereg(EPMM1, 0x30);  
  enc28j60_writereg(EPMCSL, 0xf9);  
  enc28j60_writereg(EPMCSH, 0xf7);*/

  //ʹ��MAC���� ����MAC������ͣ����֡ �����յ���ͣ����֡ʱֹͣ����
  enc28j60_writereg(MACON1, MACON1_MARXEN|MACON1_TXPAUS|MACON1_RXPAUS);
  //�˳���λ״̬
  enc28j60_writereg(MACON2, 0x00);
  //��0������ж�֡��60�ֽڳ� ��׷��һ��CRC ����CRCʹ�� ֡����У��ʹ�� MACȫ˫��ʹ��
  /* ע����ENC28J60��֧��802.3���Զ�Э�̻��ƣ� ���ԶԶ˵����翨��Ҫǿ������Ϊȫ˫�� */
  //ENC28J60_WriteOpt(ENC28J60_BIT_FIELD_SET, MACON3, MACON3_PADCFG0|MACON3_TXCRCEN|MACON3_FRMLNEN);
  enc28j60_writeop(ENC28J60_BIT_FIELD_SET, MACON3, MACON3_PADCFG0|MACON3_TXCRCEN|MACON3_FRMLNEN|MACON3_FULDPX);

  //����Ĭ��ֵ
  enc28j60_writereg(MABBIPG,0x15);
  enc28j60_writereg(MAIPGL, 0x12);
  enc28j60_writereg(MAIPGH, 0x0C);
  //enc28j60_writereg(MACON4, MACON4_DEFER);//�����类ռ��ʱenc28j60���ͻ�һֱ�ȴ�
  //enc28j60_writereg(MACLCON2, 63);

  //�������֡����
  enc28j60_writereg(MAMXFLL, MAX_FRAMELEN&0xFF);

  //д��MAC��ַ
  enc28j60_writereg(MAADR0, enc28j60_dev->dev_addr[5]);
  enc28j60_writereg(MAADR1, enc28j60_dev->dev_addr[4]);
  enc28j60_writereg(MAADR2, enc28j60_dev->dev_addr[3]);
  enc28j60_writereg(MAADR3, enc28j60_dev->dev_addr[2]);
  enc28j60_writereg(MAADR4, enc28j60_dev->dev_addr[1]);
  enc28j60_writereg(MAADR5, enc28j60_dev->dev_addr[0]);

  enc28j60_writephy(PHCON1, PHCON1_PDPXMD);//����PHYΪȫ˫��  LEDBΪ������ 
  enc28j60_writephy(PHLCON, 0xD76);	//LED״̬����
  enc28j60_writephy(PHCON2, PHCON2_HDLDIS);//�ػ���ֹ
  enc28j60_writephy(PHIE, PHIE_PLINKE | PHIE_PGEIE);//������������״̬�ı��ж�
  
  enc28j60_setbank(ECON1);
  enc28j60_writeop(ENC28J60_BIT_FIELD_SET, EIE, EIE_INTIE|EIE_PKTIE|EIR_LINKIF);//ʹ���ж� (ȫ���ж� �����ж� �����ж� ���ʹ����ж� ����״̬�ı�)
  enc28j60_writeop(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_RXEN);//����ʹ��λ

  return RT_EOK;
}


static struct pbuf *enc28j60_rx(rt_device_t dev)
{
  struct pbuf* p;
  rt_uint8_t pk_counter;//�������ݰ�����
  rt_uint8_t level;     //�жϱ�־λ
  rt_uint16_t len;      //�������ݳ���
  rt_uint16_t rxstat;   //����״̬��־λ

  p = RT_NULL;
  rt_sem_take(&lock_sem, RT_WAITING_FOREVER);   //��ȡENC28J60���ź���
  level = enc28j60_interrupt_disable();         //��ֹenc28j60���ж�

  pk_counter = enc28j60_readreg(EPKTCNT);       //��ȡenc28j60��FIFO�����ݰ��ĸ���
  if (pk_counter)
  {
    //���ý��ջ�������ָ��
    enc28j60_writereg(ERDPTL, (NextPacketPtr));
    enc28j60_writereg(ERDPTH, (NextPacketPtr)>>8);

    //����һ������ָ�루�������ݰ��ṹʾ�� �����ֲ�43ҳ��
    NextPacketPtr  = enc28j60_readop(ENC28J60_READ_BUF_MEM, 0);
    NextPacketPtr |= enc28j60_readop(ENC28J60_READ_BUF_MEM, 0)<<8;

    //�����ĳ��ȣ�����״̬���������ֽڣ���Ŀ��MAC��ַ��ʼ�����ݰ��ĳ��ȣ�
    len  = enc28j60_readop(ENC28J60_READ_BUF_MEM, 0);
    len |= enc28j60_readop(ENC28J60_READ_BUF_MEM, 0) <<8;
    len-=4;//len-4ȥ��CRCУ�鲿�� 

    //��ȡ����״̬���������ֽ�
    rxstat  = enc28j60_readop(ENC28J60_READ_BUF_MEM, 0);
    rxstat |= ((rt_uint16_t)enc28j60_readop(ENC28J60_READ_BUF_MEM, 0))<<8;

    // check CRC and symbol errors (see datasheet page 44, table 7-3):
    // The ERXFCON.CRCEN is set by default. Normally we should not
    // need to check this.
    if ((rxstat & 0x80)==0)//������ݰ�����ʧ��
      len=0;
    else
    {
      p = pbuf_alloc(PBUF_LINK, len, PBUF_RAM);//����pbuf���ڴ��enc28j60������
      if (p != RT_NULL)
      {
        rt_uint8_t* data;
        struct pbuf* q;
        for (q = p; q != RT_NULL; q= q->next)
        {
          data = q->payload;
          len = q->len;

          CSACTIVE;
          SPI_RW(ENC28J60_SPI,ENC28J60_READ_BUF_MEM);
          while(len--)
          *data++= SPI_RW(ENC28J60_SPI,0xFF);

        CSPASSIVE;
        }
      }
    }

    //�ƶ�Ӳ���������ջ����� ��ָ��
    enc28j60_writereg(ERXRDPTL, (NextPacketPtr));
    enc28j60_writereg(ERXRDPTH, (NextPacketPtr)>>8);

    //���ݰ��ݼ�
    enc28j60_writeop(ENC28J60_BIT_FIELD_SET, ECON2, ECON2_PKTDEC);
  }
  else
  {
    level |= EIE_PKTIE;//���ж��м�������ж�
  }
  enc28j60_interrupt_enable(level);     //ʹ��enc28j60���ж�
  rt_sem_release(&lock_sem);            //�ͷ�ENC28J60���ź���

  return p;
}



static rt_err_t enc28j60_tx( rt_device_t dev, struct pbuf* p)
{
  struct pbuf* q;
  rt_uint16_t len;
  rt_uint8_t* data;
  rt_uint8_t level;

  rt_sem_take(&lock_sem, RT_WAITING_FOREVER); //��ȡENC28J60���ź���
  level = enc28j60_interrupt_disable();       //��ֹenc28j60���ж�

  //���÷��ͻ�����дָ��
  enc28j60_writereg(EWRPTL, TXSTART_INIT&0xFF);
  enc28j60_writereg(EWRPTH, TXSTART_INIT>>8);

  //���÷��ͻ�����������ַ ��ֵ��Ӧ�������ݰ�����
  enc28j60_writereg(ETXNDL, (TXSTART_INIT+ p->tot_len + 1)&0xFF);
  enc28j60_writereg(ETXNDH, (TXSTART_INIT+ p->tot_len + 1)>>8);

  //����֮ǰ���Ϳ��ư���ʽ��,P39(0x00�����ʾ�������ݸ�ʽ��MACON3ȷ��)
  enc28j60_writeop(ENC28J60_WRITE_BUF_MEM, 0, 0x00);
  for (q = p; q != NULL; q = q->next)
  {
    CSACTIVE;
    SPI_RW(ENC28J60_SPI,ENC28J60_WRITE_BUF_MEM);

    len = q->len;
    data = q->payload;
    while(len--)
      SPI_RW(ENC28J60_SPI,*data++) ;
    CSPASSIVE;
  }

  //��ʼ����
  enc28j60_writeop(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);
  
  /* ��λ�����߼������⡣�μ� Rev. B4 Silicon Errata point 12. */
  if( (enc28j60_readreg(EIR) & EIR_TXERIF) )
  {
      enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS);
  }

  enc28j60_interrupt_enable(level);//ʹ��enc28j60���ж�
  rt_sem_release(&lock_sem);       //��ֹenc28j60���ж�

  return RT_EOK;
}



static rt_err_t enc28j60_open(rt_device_t dev, rt_uint16_t oflag)
{
  return RT_EOK;
}


static rt_err_t enc28j60_close(rt_device_t dev)
{
  return RT_EOK;
}


static rt_size_t enc28j60_read(rt_device_t dev, rt_off_t pos, void* buffer, rt_size_t size)
{
  rt_set_errno(-RT_ENOSYS);
  return 0;
}


static rt_size_t enc28j60_write(rt_device_t dev, rt_off_t pos, const void* buffer, rt_size_t size)
{
  rt_set_errno(-RT_ENOSYS);
  return 0;
}

static rt_err_t enc28j60_control(rt_device_t dev, rt_uint8_t cmd, void *args)
{
  switch(cmd)
  {
  case NIOCTL_GADDR:
    /* get mac address */
    if(args) 
      rt_memcpy(args, enc28j60_dev_entry.dev_addr, 6);
    else 
      return -RT_ERROR;
      break;
  default :
    break;
  }
  return RT_EOK;
}


rt_inline rt_uint8_t enc28j60_interrupt_disable()
{
  rt_uint8_t level;
  level = enc28j60_readreg(EIE);//��ȡ�ϴ����õ�enc28j60�жϱ�־λ
  enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, EIE, level);//������õ�enc28j60��־λ
  return level;
}

rt_inline void enc28j60_interrupt_enable(rt_uint8_t level)
{
  enc28j60_setbank(EIE);//����BANK��ַΪbank0
  enc28j60_writeop(ENC28J60_BIT_FIELD_SET, EIE, level);//����enc28j60�жϱ�־λ
}


static void enc28j60_writeop(rt_uint8_t op, rt_uint8_t address, rt_uint8_t data)
{
  //rt_uint32_t level = rt_hw_interrupt_disable();
  CSACTIVE;                                             //ENC28J60_CSƬѡ�ź�����
  SPI_RW(ENC28J60_SPI, op | (address & ADDR_MASK));     //д������ƼĴ������������Լ�����ȡ�����ݵ�ַ
  SPI_RW(ENC28J60_SPI,data);                            //��������
  CSPASSIVE;                                            //ENC28J60_CSƬѡ�ź�����
  //rt_hw_interrupt_enable(level);
}

static rt_uint8_t enc28j60_readop(rt_uint8_t op, rt_uint8_t address)
{
  rt_uint8_t data=0;
  CSACTIVE;
  SPI_RW(ENC28J60_SPI, op | (address & ADDR_MASK));     //д������ƼĴ������������Լ�����ȡ�����ݵ�ַ
  data=SPI_RW(ENC28J60_SPI, 0xFF);                      //��������
  if(address & SPRD_MASK)                               //MAC��MII�Ĵ����������ڵڶ����ֽڣ������ֲ�29ҳ��
    data=SPI_RW(ENC28J60_SPI, 0xFF);
  CSPASSIVE;
  return (data);
}


static void enc28j60_setbank(rt_uint8_t address)
{
  static rt_uint8_t  Enc28j60Bank;
  if((address & BANK_MASK) != Enc28j60Bank)//���BANK��ַ���͸ı䣬������BANK��ַ
  {
    enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, ECON1, (ECON1_BSEL1|ECON1_BSEL0));//���BANK��ַ���üĴ���
    enc28j60_writeop(ENC28J60_BIT_FIELD_SET, ECON1, (address & BANK_MASK)>>5); //����BANK��ַ
    Enc28j60Bank = (address & BANK_MASK);                                      //����BANK��ַ�Ļ������
  }
}


static rt_uint8_t enc28j60_readreg(rt_uint8_t address)
{
  enc28j60_setbank(address);      //����BANK��ַ
  return enc28j60_readop(ENC28J60_READ_CTRL_REG, address);//��ȡ�Ĵ�������
}


static void enc28j60_writereg(rt_uint8_t address, rt_uint8_t data)
{
  enc28j60_setbank(address);
  enc28j60_writeop(ENC28J60_WRITE_CTRL_REG, address, data);
}


static rt_uint16_t enc28j60_readphy(rt_uint8_t address)
{
  rt_uint16_t data;
  enc28j60_writereg(MIREGADR, address);//��������Ĵ�����ַ
  enc28j60_writereg(MICMD, MICMD_MIIRD);//��������Ĵ�����ʹ�ܣ���һ������Ĵ����������ݷ���MIRD�Ĵ�����
  while(enc28j60_readreg(MISTAT) & MISTAT_BUSY);//ѭ���ȴ�����Ĵ�����MII��������10.24us
  enc28j60_writereg(MICMD, 0x00);//ֹͣ��

  data=enc28j60_readreg(MIRDH);//��MIRD�ж�ȡ����Ĵ���������
  data=data<<8;
  data|=enc28j60_readreg(MIRDL);
  return data;
}


static void enc28j60_writephy(rt_uint8_t address, rt_uint16_t data)
{
  enc28j60_writereg(MIREGADR, address);//��������Ĵ�����ַ
  enc28j60_writereg(MIWRL, data);//д����Ĵ������ݣ�д��MIWRH��λ��MII�Զ�������д�뵽����Ĵ�����
  enc28j60_writereg(MIWRH, data>>8);
  while(enc28j60_readreg(MISTAT) & MISTAT_BUSY);//�ȴ�д��������
}


static void GPIO_Configure()
{
  GPIO_InitTypeDef GPIO_InitStructure;//����GPIO��ʼ���ṹ��
  EXTI_InitTypeDef EXTI_InitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;
  //����A4Ϊenc28j60_cs�ź�
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;                    
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  GPIO_WriteBit(GPIOA, GPIO_Pin_4, Bit_SET);
  
  //����C2Ϊenc28j60_int�ź�
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;                    
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOC, &GPIO_InitStructure);
  GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource2);
  EXTI_InitStructure.EXTI_Line = EXTI_Line2;
  EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
  EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
  EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  EXTI_Init(&EXTI_InitStructure);
  NVIC_InitStructure.NVIC_IRQChannel = EXTI2_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
}


static rt_bool_t enc28j60_check_link_status()
{
  rt_uint16_t reg;
  //int duplex = duplex;

  reg = enc28j60_readphy(PHSTAT2);
  //duplex = reg & PHSTAT2_DPXSTAT;

  if (reg & PHSTAT2_LSTAT)
    return RT_TRUE;
  else
    return RT_FALSE;
}



