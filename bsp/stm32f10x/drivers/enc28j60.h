/*
 * File      : enc28j60.h
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2009, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2009-01-05     Bernard      the first version
 */

#ifndef __ENC28J60_H__
#define __ENC28J60_H__

#include <rtthread.h>

// ENC28J60�Ĵ�����ַ�궨��
// - ����淶:�Ĵ�����ʵ��ַ(0-4λ)��BANK��ַ(5-6λ),����MAC��MII��ַ(��8λ)
#define ADDR_MASK        0x1F   //�Ĵ�����ַ����
#define BANK_MASK        0x60   //�洢��������
#define SPRD_MASK        0x80   //MAC��MII�Ĵ�������

/***********************SPI����ָ�***********************/
#define ENC28J60_READ_CTRL_REG       0x00       //�����ƼĴ���
#define ENC28J60_READ_BUF_MEM        0x3A       //��������
#define ENC28J60_WRITE_CTRL_REG      0x40       //д���ƼĴ���
#define ENC28J60_WRITE_BUF_MEM       0x7A       //д������
#define ENC28J60_BIT_FIELD_SET       0x80       //λ����1
#define ENC28J60_BIT_FIELD_CLR       0xA0       //λ����0
#define ENC28J60_SOFT_RESET          0xFF       //ϵͳ��������λ

/***********************���ƼĴ����궨��***********************/
//����BANK�����Ĵ���
#define EIE              0x1B
        #define EIE_INTIE        0x80
        #define EIE_PKTIE        0x40
        #define EIE_DMAIE        0x20
        #define EIE_LINKIE       0x10
        #define EIE_TXIE         0x08
        #define EIE_WOLIE        0x04
        #define EIE_TXERIE       0x02
        #define EIE_RXERIE       0x01
#define EIR              0x1C
        #define EIR_PKTIF        0x40
        #define EIR_DMAIF        0x20
        #define EIR_LINKIF       0x10
        #define EIR_TXIF         0x08
        #define EIR_WOLIF        0x04
        #define EIR_TXERIF       0x02
        #define EIR_RXERIF       0x01
#define ESTAT            0x1D
        #define ESTAT_INT        0x80
        #define ESTAT_LATECOL    0x10
        #define ESTAT_RXBUSY     0x04
        #define ESTAT_TXABRT     0x02
        #define ESTAT_CLKRDY     0x01
#define ECON2            0x1E
        #define ECON2_AUTOINC    0x80
        #define ECON2_PKTDEC     0x40
        #define ECON2_PWRSV      0x20
        #define ECON2_VRPS       0x08
#define ECON1            0x1F
        #define ECON1_TXRST      0x80
        #define ECON1_RXRST      0x40
        #define ECON1_DMAST      0x20
        #define ECON1_CSUMEN     0x10
        #define ECON1_TXRTS      0x08
        #define ECON1_RXEN       0x04
        #define ECON1_BSEL1      0x02
        #define ECON1_BSEL0      0x01
// Bank0�Ĵ���
#define ERDPTL           (0x00|0x00)
#define ERDPTH           (0x01|0x00)
#define EWRPTL           (0x02|0x00)
#define EWRPTH           (0x03|0x00)
#define ETXSTL           (0x04|0x00)
#define ETXSTH           (0x05|0x00)
#define ETXNDL           (0x06|0x00)
#define ETXNDH           (0x07|0x00)
#define ERXSTL           (0x08|0x00)
#define ERXSTH           (0x09|0x00)
#define ERXNDL           (0x0A|0x00)
#define ERXNDH           (0x0B|0x00)
#define ERXRDPTL         (0x0C|0x00)
#define ERXRDPTH         (0x0D|0x00)
#define ERXWRPTL         (0x0E|0x00)
#define ERXWRPTH         (0x0F|0x00)
#define EDMASTL          (0x10|0x00)
#define EDMASTH          (0x11|0x00)
#define EDMANDL          (0x12|0x00)
#define EDMANDH          (0x13|0x00)
#define EDMADSTL         (0x14|0x00)
#define EDMADSTH         (0x15|0x00)
#define EDMACSL          (0x16|0x00)
#define EDMACSH          (0x17|0x00)
// Bank1�Ĵ���
#define EHT0             (0x00|0x20)
#define EHT1             (0x01|0x20)
#define EHT2             (0x02|0x20)
#define EHT3             (0x03|0x20)
#define EHT4             (0x04|0x20)
#define EHT5             (0x05|0x20)
#define EHT6             (0x06|0x20)
#define EHT7             (0x07|0x20)
#define EPMM0            (0x08|0x20)
#define EPMM1            (0x09|0x20)
#define EPMM2            (0x0A|0x20)
#define EPMM3            (0x0B|0x20)
#define EPMM4            (0x0C|0x20)
#define EPMM5            (0x0D|0x20)
#define EPMM6            (0x0E|0x20)
#define EPMM7            (0x0F|0x20)
#define EPMCSL           (0x10|0x20)
#define EPMCSH           (0x11|0x20)
#define EPMOL            (0x14|0x20)
#define EPMOH            (0x15|0x20)
#define EWOLIE           (0x16|0x20)
#define EWOLIR           (0x17|0x20)
#define ERXFCON          (0x18|0x20)
        #define ERXFCON_UCEN     0x80
        #define ERXFCON_ANDOR    0x40
        #define ERXFCON_CRCEN    0x20
        #define ERXFCON_PMEN     0x10
        #define ERXFCON_MPEN     0x08
        #define ERXFCON_HTEN     0x04
        #define ERXFCON_MCEN     0x02
        #define ERXFCON_BCEN     0x01
#define EPKTCNT          (0x19|0x20)
// Bank2�Ĵ���
#define MACON1           (0x00|0x40|0x80)
        #define MACON1_LOOPBK    0x10
        #define MACON1_TXPAUS    0x08
        #define MACON1_RXPAUS    0x04
        #define MACON1_PASSALL   0x02
        #define MACON1_MARXEN    0x01
#define MACON2           (0x01|0x40|0x80)
        #define MACON2_MARST     0x80
        #define MACON2_RNDRST    0x40
        #define MACON2_MARXRST   0x08
        #define MACON2_RFUNRST   0x04
        #define MACON2_MATXRST   0x02
        #define MACON2_TFUNRST   0x01
#define MACON3           (0x02|0x40|0x80)
        #define MACON3_PADCFG2   0x80
        #define MACON3_PADCFG1   0x40
        #define MACON3_PADCFG0   0x20
        #define MACON3_TXCRCEN   0x10
        #define MACON3_PHDRLEN   0x08
        #define MACON3_HFRMLEN   0x04
        #define MACON3_FRMLNEN   0x02
        #define MACON3_FULDPX    0x01
#define MACON4           (0x03|0x40|0x80)
        #define	MACON4_DEFER	(1<<6)
        #define	MACON4_BPEN     (1<<5)
        #define	MACON4_NOBKOFF	(1<<4)
#define MABBIPG          (0x04|0x40|0x80)
#define MAIPGL           (0x06|0x40|0x80)
#define MAIPGH           (0x07|0x40|0x80)
#define MACLCON1         (0x08|0x40|0x80)
#define MACLCON2         (0x09|0x40|0x80)
#define MAMXFLL          (0x0A|0x40|0x80)
#define MAMXFLH          (0x0B|0x40|0x80)
#define MAPHSUP          (0x0D|0x40|0x80)
#define MICON            (0x11|0x40|0x80)
#define MICMD            (0x12|0x40|0x80)
        #define MICMD_MIISCAN    0x02
        #define MICMD_MIIRD      0x01
#define MIREGADR         (0x14|0x40|0x80)
#define MIWRL            (0x16|0x40|0x80)
#define MIWRH            (0x17|0x40|0x80)
#define MIRDL            (0x18|0x40|0x80)
#define MIRDH            (0x19|0x40|0x80)
// Bank3�Ĵ���
#define MAADR1           (0x00|0x60|0x80)
#define MAADR0           (0x01|0x60|0x80)
#define MAADR3           (0x02|0x60|0x80)
#define MAADR2           (0x03|0x60|0x80)
#define MAADR5           (0x04|0x60|0x80)
#define MAADR4           (0x05|0x60|0x80)
#define EBSTSD           (0x06|0x60)
#define EBSTCON          (0x07|0x60)
#define EBSTCSL          (0x08|0x60)
#define EBSTCSH          (0x09|0x60)
#define MISTAT           (0x0A|0x60|0x80)
        #define MISTAT_NVALID    0x04
        #define MISTAT_SCAN      0x02
        #define MISTAT_BUSY      0x01
#define EREVID           (0x12|0x60)
#define ECOCON           (0x15|0x60)
#define EFLOCON          (0x17|0x60)
#define EPAUSL           (0x18|0x60)
#define EPAUSH           (0x19|0x60)

/***********************����Ĵ����궨��***********************/
#define PHCON1           0x00
        #define PHCON1_PRST      0x8000
        #define PHCON1_PLOOPBK   0x4000
        #define PHCON1_PPWRSV    0x0800
        #define PHCON1_PDPXMD    0x0100
#define PHSTAT1          0x01
        #define PHSTAT1_PFDPX    0x1000
        #define PHSTAT1_PHDPX    0x0800
        #define PHSTAT1_LLSTAT   0x0004
        #define PHSTAT1_JBSTAT   0x0002
#define PHID1            0x02
#define PHID2            0x03
#define PHCON2           0x10
        #define PHCON2_FRCLINK   0x4000
        #define PHCON2_TXDIS     0x2000
        #define PHCON2_JABBER    0x0400
        #define PHCON2_HDLDIS    0x0100
#define PHSTAT2          0x11
        #define PHSTAT2_TXSTAT	 0x2000
        #define PHSTAT2_RXSTAT	 0x1000
        #define PHSTAT2_COLSTAT	 0x0800
        #define PHSTAT2_LSTAT	 0x0400
        #define PHSTAT2_DPXSTAT	 0x0200
        #define PHSTAT2_PLRITY	 0x0020
#define PHIE             0x12
        #define PHIE_PLINKE	 0x0010
        #define PHIE_PGEIE	 0x0002
#define PHIR             0x13
#define PHLCON           0x14



//ע������̫��Э���У����ı��ĳ���Ϊ1518�ֽڣ�����С���ĳ���Ϊ60�ֽڡ�
//    ���ͻ��������ڻ��Դ���1518�ֽڣ�ʣ��Ĳ���ȫ����������ջ�������
#define RXSTART_INIT	0x0000                  //���ջ�������ʼ��ַ
#define RXSTOP_INIT	(0x1FFF-0x0600-1)       //���ջ�����ֹͣ��ַ(���ܻ�������С0x1A00) 
#define TXSTART_INIT	(0x1FFF-0x0600)         //���ͻ�������ʼ��ַ
#define TXSTOP_INIT	0x1FFF                  //���ͻ�����ֹͣ��ַ(���ܻ�������С0x600) 

#define	MAX_FRAMELEN	1518	//���ı��ĳ���Ϊ1518�ֽ�

int rt_hw_enc28j60_init(void);

#endif

