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

//ENC28J60的CS引脚使用PTA4
#define ENC28J60_SPI    SPI1                    //ENC28J60使用SPI1模块        
#define CSACTIVE 	GPIOA->BRR = GPIO_Pin_4; //ENC28J60_CS拉低
#define CSPASSIVE	GPIOA->BSRR = GPIO_Pin_4;//ENC28J60_CS拉高

struct net_device
{
  /* inherit from ethernet device */
  struct eth_device parent;

  rt_uint8_t  dev_addr[MAX_ADDR_LEN];//MAC地址
};


static struct net_device  enc28j60_dev_entry;                   //创建一个网络设备对象
static struct net_device *enc28j60_dev =&enc28j60_dev_entry;    //创建指向网络设备对象的指针
static rt_uint16_t NextPacketPtr;                               //存储下一个数据包的首地址
static struct rt_semaphore lock_sem;                            //定义enc28j60的信号量对象

//私有函数声明
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
  SPI_INIT(ENC28J60_SPI);                                 //SPI初始化

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

  rt_sem_init(&lock_sem, "lock", 1, RT_IPC_FLAG_FIFO);//创建一个静态信号量
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
  eir = enc28j60_readreg(EIR);//获取enc28j60中断请求标志位
  do
  {
    pk_counter = enc28j60_readreg(EPKTCNT);//获取enc28j60缓冲区中数据包的个数
    if (pk_counter)
    {
      eth_device_ready((struct eth_device*)&(enc28j60_dev->parent));//发送邮件，表示数据帧接收到
      enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, EIE, EIE_PKTIE);//禁止接收中断
    }
    if (eir & EIR_PKTIF)//接收到数据包
    {
      rx_activiated = RT_TRUE;
    }
    if ((eir & EIR_LINKIF) != 0)//连接状态改变中断
    {
      rt_bool_t up=enc28j60_check_link_status();
      eth_device_linkchange((struct eth_device*)&(enc28j60_dev->parent), up);
      enc28j60_readphy(PHIR);//读取PHIR寄存器 清除链接状态改变中断标志位
    }
    
    
    if (eir & EIR_TXIF)//发送中断
    {
      enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, EIR, EIR_TXIF);//清除发送中断标志位
    }
    if ((eir & EIR_TXERIF) != 0)//发送错误中断
    {
      enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, EIR, EIR_TXERIF);//清除发送错误中断标志位
    }
    if ((eir & EIR_RXERIF) != 0)//接收错误中断
    {
      enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, EIR, EIR_RXERIF);//清除接收错误中断标志位
    }
    if ((eir & EIR_DMAIF) != 0)//DMA中断
    {
      enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, EIR, EIR_DMAIF);//清除DMA断标志位
    }
    if ((eir & EIR_WOLIF) != 0)//WOL中断
    {
      enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, EIR, EIR_WOLIF);//清除WOL中断标志位
    }
    eir = enc28j60_readreg(EIR);//获取enc28j60中断请求标志位
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
  enc28j60_writeop(ENC28J60_SOFT_RESET,0,ENC28J60_SOFT_RESET);//系统软复位
  while(!(enc28j60_readreg(ESTAT) & ESTAT_CLKRDY));//查询ESTAT.CLKRDY位,等待时钟就绪

  //设置接收缓冲区起始地址
  NextPacketPtr = RXSTART_INIT;
  enc28j60_writereg(ERXSTL, RXSTART_INIT&0xFF);
  enc28j60_writereg(ERXSTH, RXSTART_INIT>>8);
  //设置禁止硬件接收写入FIFO的地址
  enc28j60_writereg(ERXRDPTL, RXSTOP_INIT&0xFF);
  enc28j60_writereg(ERXRDPTH, RXSTOP_INIT>>8);
  //设置接收缓冲区结束地址
  enc28j60_writereg(ERXNDL, RXSTOP_INIT&0xFF);
  enc28j60_writereg(ERXNDH, RXSTOP_INIT>>8);

  //设置发送缓冲区起始地址
  enc28j60_writereg(ETXSTL, TXSTART_INIT&0xFF);
  enc28j60_writereg(ETXSTH, TXSTART_INIT>>8);
  //设置发送缓冲区写指针
  enc28j60_writereg(EWRPTL, TXSTART_INIT&0xFF);
  enc28j60_writereg(EWRPTH, TXSTART_INIT>>8);
  //设置发送缓冲区结束地址
  enc28j60_writereg(ETXNDL, TXSTOP_INIT&0xFF);
  enc28j60_writereg(ETXNDH, TXSTOP_INIT>>8);

//使能单播过滤 使能CRC校验 使能 格式匹配自动过滤 
  enc28j60_writereg(ERXFCON, ERXFCON_UCEN|ERXFCON_CRCEN|ERXFCON_BCEN);
  /*enc28j60_writereg(ERXFCON, ERXFCON_UCEN|ERXFCON_CRCEN|ERXFCON_PMEN);  
  enc28j60_writereg(EPMM0, 0x3f);  
  enc28j60_writereg(EPMM1, 0x30);  
  enc28j60_writereg(EPMCSL, 0xf9);  
  enc28j60_writereg(EPMCSH, 0xf7);*/

  //使能MAC接收 允许MAC发送暂停控制帧 当接收到暂停控制帧时停止发送
  enc28j60_writereg(MACON1, MACON1_MARXEN|MACON1_TXPAUS|MACON1_RXPAUS);
  //退出复位状态
  enc28j60_writereg(MACON2, 0x00);
  //用0填充所有短帧至60字节长 并追加一个CRC 发送CRC使能 帧长度校验使能 MAC全双工使能
  /* 注由于ENC28J60不支持802.3的自动协商机制， 所以对端的网络卡需要强制设置为全双工 */
  //ENC28J60_WriteOpt(ENC28J60_BIT_FIELD_SET, MACON3, MACON3_PADCFG0|MACON3_TXCRCEN|MACON3_FRMLNEN);
  enc28j60_writeop(ENC28J60_BIT_FIELD_SET, MACON3, MACON3_PADCFG0|MACON3_TXCRCEN|MACON3_FRMLNEN|MACON3_FULDPX);

  //填入默认值
  enc28j60_writereg(MABBIPG,0x15);
  enc28j60_writereg(MAIPGL, 0x12);
  enc28j60_writereg(MAIPGH, 0x0C);
  //enc28j60_writereg(MACON4, MACON4_DEFER);//当网络被占用时enc28j60发送会一直等待
  //enc28j60_writereg(MACLCON2, 63);

  //设置最大帧长度
  enc28j60_writereg(MAMXFLL, MAX_FRAMELEN&0xFF);

  //写入MAC地址
  enc28j60_writereg(MAADR0, enc28j60_dev->dev_addr[5]);
  enc28j60_writereg(MAADR1, enc28j60_dev->dev_addr[4]);
  enc28j60_writereg(MAADR2, enc28j60_dev->dev_addr[3]);
  enc28j60_writereg(MAADR3, enc28j60_dev->dev_addr[2]);
  enc28j60_writereg(MAADR4, enc28j60_dev->dev_addr[1]);
  enc28j60_writereg(MAADR5, enc28j60_dev->dev_addr[0]);

  enc28j60_writephy(PHCON1, PHCON1_PDPXMD);//配置PHY为全双工  LEDB为拉电流 
  enc28j60_writephy(PHLCON, 0xD76);	//LED状态设置
  enc28j60_writephy(PHCON2, PHCON2_HDLDIS);//回环禁止
  enc28j60_writephy(PHIE, PHIE_PLINKE | PHIE_PGEIE);//开启物理连接状态改变中断
  
  enc28j60_setbank(ECON1);
  enc28j60_writeop(ENC28J60_BIT_FIELD_SET, EIE, EIE_INTIE|EIE_PKTIE|EIR_LINKIF);//使能中断 (全局中断 接收中断 发送中断 发送错误中断 链接状态改变)
  enc28j60_writeop(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_RXEN);//接收使能位

  return RT_EOK;
}


static struct pbuf *enc28j60_rx(rt_device_t dev)
{
  struct pbuf* p;
  rt_uint8_t pk_counter;//接收数据包个数
  rt_uint8_t level;     //中断标志位
  rt_uint16_t len;      //接收数据长度
  rt_uint16_t rxstat;   //接收状态标志位

  p = RT_NULL;
  rt_sem_take(&lock_sem, RT_WAITING_FOREVER);   //获取ENC28J60的信号量
  level = enc28j60_interrupt_disable();         //禁止enc28j60的中断

  pk_counter = enc28j60_readreg(EPKTCNT);       //获取enc28j60的FIFO中数据包的个数
  if (pk_counter)
  {
    //设置接收缓冲器读指针
    enc28j60_writereg(ERDPTL, (NextPacketPtr));
    enc28j60_writereg(ERDPTH, (NextPacketPtr)>>8);

    //读下一个包的指针（接收数据包结构示例 数据手册43页）
    NextPacketPtr  = enc28j60_readop(ENC28J60_READ_BUF_MEM, 0);
    NextPacketPtr |= enc28j60_readop(ENC28J60_READ_BUF_MEM, 0)<<8;

    //读包的长度（接收状态的两个低字节，从目标MAC地址开始的数据包的长度）
    len  = enc28j60_readop(ENC28J60_READ_BUF_MEM, 0);
    len |= enc28j60_readop(ENC28J60_READ_BUF_MEM, 0) <<8;
    len-=4;//len-4去除CRC校验部分 

    //读取接收状态的两个高字节
    rxstat  = enc28j60_readop(ENC28J60_READ_BUF_MEM, 0);
    rxstat |= ((rt_uint16_t)enc28j60_readop(ENC28J60_READ_BUF_MEM, 0))<<8;

    // check CRC and symbol errors (see datasheet page 44, table 7-3):
    // The ERXFCON.CRCEN is set by default. Normally we should not
    // need to check this.
    if ((rxstat & 0x80)==0)//如果数据包接收失败
      len=0;
    else
    {
      p = pbuf_alloc(PBUF_LINK, len, PBUF_RAM);//申请pbuf用于存放enc28j60的数据
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

    //移动硬件保护接收缓冲区 读指针
    enc28j60_writereg(ERXRDPTL, (NextPacketPtr));
    enc28j60_writereg(ERXRDPTH, (NextPacketPtr)>>8);

    //数据包递减
    enc28j60_writeop(ENC28J60_BIT_FIELD_SET, ECON2, ECON2_PKTDEC);
  }
  else
  {
    level |= EIE_PKTIE;//在中断中加入接收中断
  }
  enc28j60_interrupt_enable(level);     //使能enc28j60的中断
  rt_sem_release(&lock_sem);            //释放ENC28J60的信号量

  return p;
}



static rt_err_t enc28j60_tx( rt_device_t dev, struct pbuf* p)
{
  struct pbuf* q;
  rt_uint16_t len;
  rt_uint8_t* data;
  rt_uint8_t level;

  rt_sem_take(&lock_sem, RT_WAITING_FOREVER); //获取ENC28J60的信号量
  level = enc28j60_interrupt_disable();       //禁止enc28j60的中断

  //设置发送缓冲区写指针
  enc28j60_writereg(EWRPTL, TXSTART_INIT&0xFF);
  enc28j60_writereg(EWRPTH, TXSTART_INIT>>8);

  //设置发送缓冲区结束地址 该值对应发送数据包长度
  enc28j60_writereg(ETXNDL, (TXSTART_INIT+ p->tot_len + 1)&0xFF);
  enc28j60_writereg(ETXNDH, (TXSTART_INIT+ p->tot_len + 1)>>8);

  //发送之前发送控制包格式字,P39(0x00命令表示发送数据格式由MACON3确定)
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

  //开始发送
  enc28j60_writeop(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);
  
  /* 复位发送逻辑的问题。参见 Rev. B4 Silicon Errata point 12. */
  if( (enc28j60_readreg(EIR) & EIR_TXERIF) )
  {
      enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS);
  }

  enc28j60_interrupt_enable(level);//使能enc28j60的中断
  rt_sem_release(&lock_sem);       //禁止enc28j60的中断

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
  level = enc28j60_readreg(EIE);//获取上次设置的enc28j60中断标志位
  enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, EIE, level);//清除设置的enc28j60标志位
  return level;
}

rt_inline void enc28j60_interrupt_enable(rt_uint8_t level)
{
  enc28j60_setbank(EIE);//设置BANK地址为bank0
  enc28j60_writeop(ENC28J60_BIT_FIELD_SET, EIE, level);//设置enc28j60中断标志位
}


static void enc28j60_writeop(rt_uint8_t op, rt_uint8_t address, rt_uint8_t data)
{
  //rt_uint32_t level = rt_hw_interrupt_disable();
  CSACTIVE;                                             //ENC28J60_CS片选信号拉低
  SPI_RW(ENC28J60_SPI, op | (address & ADDR_MASK));     //写入读控制寄存器操作命令以及待读取的数据地址
  SPI_RW(ENC28J60_SPI,data);                            //读出数据
  CSPASSIVE;                                            //ENC28J60_CS片选信号拉高
  //rt_hw_interrupt_enable(level);
}

static rt_uint8_t enc28j60_readop(rt_uint8_t op, rt_uint8_t address)
{
  rt_uint8_t data=0;
  CSACTIVE;
  SPI_RW(ENC28J60_SPI, op | (address & ADDR_MASK));     //写入读控制寄存器操作命令以及待读取的数据地址
  data=SPI_RW(ENC28J60_SPI, 0xFF);                      //读出数据
  if(address & SPRD_MASK)                               //MAC和MII寄存器的数据在第二个字节（数据手册29页）
    data=SPI_RW(ENC28J60_SPI, 0xFF);
  CSPASSIVE;
  return (data);
}


static void enc28j60_setbank(rt_uint8_t address)
{
  static rt_uint8_t  Enc28j60Bank;
  if((address & BANK_MASK) != Enc28j60Bank)//如果BANK地址发送改变，则设置BANK地址
  {
    enc28j60_writeop(ENC28J60_BIT_FIELD_CLR, ECON1, (ECON1_BSEL1|ECON1_BSEL0));//清空BANK地址设置寄存器
    enc28j60_writeop(ENC28J60_BIT_FIELD_SET, ECON1, (address & BANK_MASK)>>5); //设置BANK地址
    Enc28j60Bank = (address & BANK_MASK);                                      //更新BANK地址的缓存变量
  }
}


static rt_uint8_t enc28j60_readreg(rt_uint8_t address)
{
  enc28j60_setbank(address);      //设置BANK地址
  return enc28j60_readop(ENC28J60_READ_CTRL_REG, address);//读取寄存器数据
}


static void enc28j60_writereg(rt_uint8_t address, rt_uint8_t data)
{
  enc28j60_setbank(address);
  enc28j60_writeop(ENC28J60_WRITE_CTRL_REG, address, data);
}


static rt_uint16_t enc28j60_readphy(rt_uint8_t address)
{
  rt_uint16_t data;
  enc28j60_writereg(MIREGADR, address);//设置物理寄存器地址
  enc28j60_writereg(MICMD, MICMD_MIIRD);//启动物理寄存器读使能，读一次物理寄存器并将数据放入MIRD寄存器中
  while(enc28j60_readreg(MISTAT) & MISTAT_BUSY);//循环等待物理寄存器被MII读出，需10.24us
  enc28j60_writereg(MICMD, 0x00);//停止读

  data=enc28j60_readreg(MIRDH);//从MIRD中读取物理寄存器的数据
  data=data<<8;
  data|=enc28j60_readreg(MIRDL);
  return data;
}


static void enc28j60_writephy(rt_uint8_t address, rt_uint16_t data)
{
  enc28j60_writereg(MIREGADR, address);//设置物理寄存器地址
  enc28j60_writereg(MIWRL, data);//写物理寄存器数据，写完MIWRH高位后，MII自动将数据写入到物理寄存器中
  enc28j60_writereg(MIWRH, data>>8);
  while(enc28j60_readreg(MISTAT) & MISTAT_BUSY);//等待写操作结束
}


static void GPIO_Configure()
{
  GPIO_InitTypeDef GPIO_InitStructure;//定义GPIO初始化结构体
  EXTI_InitTypeDef EXTI_InitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;
  //设置A4为enc28j60_cs信号
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;                    
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  GPIO_WriteBit(GPIOA, GPIO_Pin_4, Bit_SET);
  
  //设置C2为enc28j60_int信号
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



