/*
 * File      : application.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2009-01-05     Bernard      the first version
 */

#include <rtthread.h>
#include <stdio.h>
#include <board.h>
#include <RauAp.h>
#include <lwip/netdb.h> 
#include <lwip/sockets.h>
#include <RauTimer.h>

#define BUFSZ	1024
#define SERVER_URL      "42.96.202.37"
#define SERVER_PORT   8400      


#define RCU_UART 
#define hclog   rt_kprintf
#define hlog    hdp_client_log
static struct rt_semaphore rcuReadSem;
static struct rt_device * rcuDevice;
#define MAX_LOG_LEN 128



SOCKET RauSock =0;
struct member_list_t member_list[10];
static unsigned int gclient_cmd_index =0xffffffff;
int g_member_num =0;
char send_buf[SEND_BUF_LEN];
char recv_buf[RECV_BUG_LEN];
//extern void rt_thread_switch_hook(struct rt_thread *from, struct rt_thread *to);

int rau_app_analysis_data(SOCKET client,char *pbuf,int len)
{

    int ret = -1;
    short cmd = 0,cmd_h=0xffff,cmd_l=0xffff, ver = 0;	
    int cmd_index = 0;
    int datalen = 0;	
    LClock_t EndClock;
    
    if(*pbuf!=0x02)
    {
    	hclog("消息头错误\n");
        ret = HEAD_ERROR;
    	return ret;
    }
    pbuf++;
    ver = make_int16(pbuf);
    hclog("protocol version number:%d\n",(unsigned int)ver);
    pbuf+=2;

    cmd = make_int16(pbuf);
    hclog("command number:0x%02x\n",(unsigned int)cmd);
    cmd_h=cmd&0xff00;
    cmd_l=cmd&0x00ff;
    pbuf+=2;

    cmd_index = make_int32(pbuf);
    hclog("command index:%d\n",(unsigned int)cmd_index);
    pbuf+=4;

    datalen = make_int32(pbuf);
    hclog("datalen:%d\n",(unsigned int)datalen);
    pbuf+=4;

    ret = cmd_l;
    if((cmd_l == CMD_LOGIN) && (cmd_h ==CMD_RESPONSE) && (cmd_index==gclient_cmd_index))/*登陆成功*/
    {
        hlog("login success\n");
        client_get_member_list(client);
    } 
    else if((cmd_l == CMD_GET_MEMBER_LIST) && (cmd_h ==CMD_RESPONSE) && (cmd_index==gclient_cmd_index))/*返回成员列表*/
    {
        char com_rbuf[MAX_LOG_LEN]={0};
        int rlen = 0;        
        rt_memset(com_rbuf,0,MAX_LOG_LEN);
        hlog("return member\n");
        client_save_member_list(pbuf,datalen);
    }
    else if((cmd_l == CMD_SEND_MESSAGE) && (cmd_h ==CMD_RESPONSE) /*&& (cmd_index==gclient_cmd_index)*/)/*消息发送成功*/
    {
        hlog("send message success\n");
    }
    else if((cmd_l == CMD_FORWARD_MESSAGE) && (cmd_h ==0))
    {
        hlog("receive message\n");
        EndClock=GetClock();
        hlog("UsedClock:%lf\n",EndClock);        
        //printf_buf(pbuf,datalen);
        client_recvice_message(pbuf,datalen);
        client_send_receive_success(client,pbuf,cmd_index);
        
    }
    else
    {
    	hclog("unknow cmd number received:%d",(unsigned int)cmd);
    }

    return ret;
    
}

void rau_app_tcp_connect_server(void)
{
    struct hostent *host;
    struct sockaddr_in server_addr;
    int ncount=0;
    host = gethostbyname(SERVER_URL);
    while(1)
    {
        if(ncount>=5)
        {   
            hlog("尝试联接5次失败\n");
            return;
        }
        if ((RauSock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {

            hlog("RauSocket create error\n");
            return;
        }


        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SERVER_PORT);
        server_addr.sin_addr = *((struct in_addr *)host->h_addr);
        rt_memset(&(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));

        if (connect(RauSock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
        {
            hlog("Connect fail!\n");
            lwip_close(RauSock);
            rt_thread_delay(100);        				
            ncount++;
            continue;
        }
        else
        {
            hlog("联接成功开始登陆请求\n");     
            rau_app_client_login(RauSock);
            break;
        }
    }

    
}
/***************************************************************************
*
*
***************************************************************************/
short make_int16(char* ptr)
{
	short ret = 0;
	short temp = 0;//(short)*ptr;
	memcpy((char*)&temp,(char*)ptr,2);
	ret = ntohs(temp);
	return ret;
}
short make_int32(char* ptr)
{
	int ret = 0;
	int temp = 0;//(int)*ptr;
	memcpy((char*)&temp,(char*)ptr,4);
	ret = ntohl(temp);
	return ret;
}

void make_net16(char* p, short d)
{
    unsigned short t = htons((unsigned short)d);
    memcpy((char*)p,(char*)&t,2);
}
void make_net32(char* p, int d)
{
    unsigned int t = htonl((unsigned int)d);
    memcpy((char*)p,(char*)&t,4);
}

void printf_buf(char *pbuf,int len)
{
	int n =0;

	for(n=0;n<len;n++)
	{
		hclog("0x%02x ",*pbuf);	
		pbuf++;
	}
	hclog("\n");	
}

/***************************************************************************
*发送客户端登陆请求
***************************************************************************/
void client_fill_in_head(short cmd)
{
	char *pbuf = NULL;
	
	if(gclient_cmd_index == 0xffffffff)
	{
		gclient_cmd_index =0x0;
	}
	else
	{
		gclient_cmd_index++;
	}
	
	memset(send_buf,0,SEND_BUF_LEN);	
	pbuf = send_buf;
	*pbuf=0x02;
	pbuf++;
	make_net16(pbuf,PROTOCOL_VER_NUB);
	pbuf+=2;
	make_net16(pbuf,cmd);
	pbuf+=2;
	make_net32(pbuf,gclient_cmd_index);
	pbuf+=4;
}

void client_fill_in_head_with_index(short cmd,int cmd_index)
{
	char *pbuf = NULL;
	
	if(gclient_cmd_index == 0xffffffff)
	{
		gclient_cmd_index =0x0;
	}
	else
	{
		gclient_cmd_index++;
	}
	
	memset(send_buf,0,SEND_BUF_LEN);	
	pbuf = send_buf;
	*pbuf=0x02;
	pbuf++;
	make_net16(pbuf,PROTOCOL_VER_NUB);
	pbuf+=2;
	make_net16(pbuf,cmd);
	pbuf+=2;
	make_net32(pbuf,cmd_index);
	pbuf+=4;
}



void rau_app_client_login(SOCKET client)
{
    char *pbuf = NULL;
    int data_len =0;
    int err;

    pbuf = send_buf;	
    client_fill_in_head(CMD_LOGIN);
    pbuf+=(PROTOCOL_HEAD_LEN-4);	
    make_net32(pbuf,CLIENT_NAME_LEN+CLIENT_PASSWORD_LEN);
    pbuf+=4;	
    strncpy(pbuf,CLIENT_NAME,CLIENT_NAME_LEN);
    pbuf+=CLIENT_NAME_LEN;
    strncpy(pbuf,CLIENT_PASSWORD,CLIENT_PASSWORD_LEN);
    pbuf+=CLIENT_PASSWORD_LEN;
    *pbuf = 0x03;
    data_len = PROTOCOL_HEAD_LEN+CLIENT_NAME_LEN+CLIENT_PASSWORD_LEN+1;
    //printf_buf(send_buf,data_len);
    err = send(client,send_buf,data_len,0);
    if(err == SOCKET_ERROR)
    {
    	hclog("send 失败\n");	
    }
    else
    {
    	hclog("send 成功\n");
    	
    }
}


/***************************************************************************
*发送获取成员列表请求
***************************************************************************/
int client_get_member_list(SOCKET client)
{
	char *pbuf = NULL;
	int data_len =0;
	int err;

	pbuf = send_buf;	
	client_fill_in_head(CMD_GET_MEMBER_LIST);
	pbuf+=PROTOCOL_HEAD_LEN-4;
	make_net32(pbuf,0);	
	pbuf+=4;
	*pbuf = 0x03;
	data_len = PROTOCOL_HEAD_LEN+1;
	//printf_buf(send_buf,data_len);
	err = send(client,send_buf,data_len,0);
	return err;
}
/***************************************************************************
*保存在线的成员及成员状态
***************************************************************************/
void client_save_member_list(char *pbuf,int data_len)
{
	int member_num =0;
	int member_len =0;
	int i;
	member_len = CLIENT_NAME_LEN+1;
	member_num = data_len/member_len;
	for(i=0;i<member_num;i++)
	{
		hclog("member:%s \n",pbuf);	
		strncpy(member_list[i].client_name,pbuf,CLIENT_NAME_LEN);
		pbuf +=CLIENT_NAME_LEN;
		member_list[i].client_state = *pbuf;
		pbuf++; 
		g_member_num ++;
	}
}
/***************************************************************************
*给指定成员发送消息
***************************************************************************/
int client_send_message(SOCKET client,char *p_name,char *p_sting)
{
	char *pbuf = NULL;
	int data_len =0;
	int err;
	int msg_len =0;

	msg_len =strlen(p_sting);
	*(p_sting+msg_len) = '\0';
	pbuf = send_buf;	
	client_fill_in_head(CMD_SEND_MESSAGE);
	pbuf+=PROTOCOL_HEAD_LEN-4;	
	make_net32(pbuf,CLIENT_NAME_LEN*2+strlen(p_sting)+1);
	pbuf+=4;	
	strncpy(pbuf,CLIENT_NAME,CLIENT_NAME_LEN);
	pbuf +=CLIENT_NAME_LEN;
	strncpy(pbuf,p_name,CLIENT_NAME_LEN);
	pbuf +=CLIENT_NAME_LEN;	
	strncpy(pbuf,p_sting,msg_len+1);	
	pbuf +=msg_len+1;		
	*pbuf = 0x03;
	data_len =CLIENT_NAME_LEN*2+msg_len+1+PROTOCOL_HEAD_LEN+1;
	//printf_buf(send_buf,data_len);
	err = send(client,send_buf,data_len,0);
	return err; 
}

/***************************************************************************
*发送接收信息成功
***************************************************************************/
int client_send_receive_success(SOCKET client,char *pdata,int cmd_index)
{
	char *pbuf = NULL;
	int err;
	int data_len =0;
		
	pbuf = send_buf;	
	client_fill_in_head_with_index(CMD_FORWARD_MESSAGE|0x0100,cmd_index);
	pbuf+=PROTOCOL_HEAD_LEN-4;	
	make_net32(pbuf,CLIENT_NAME_LEN*2+strlen("ACT_SUCCESS"));
	pbuf+=4;	
	strncpy(pbuf,pdata,CLIENT_NAME_LEN*2);
	pbuf +=CLIENT_NAME_LEN*2;
	strncpy(pbuf,"ACT_SUCCESS",strlen("ACT_SUCCESS"));	
	pbuf +=strlen("ACT_SUCCESS");		
	*pbuf = 0x03;
	data_len =CLIENT_NAME_LEN*2+strlen("ACT_SUCCESS")+PROTOCOL_HEAD_LEN+1;
	//printf_buf(send_buf,data_len);
	err = send(client,send_buf,data_len,0);
	return err;
}




/***************************************************************************
*显示接收消息内存
***************************************************************************/
void client_recvice_message(char *pbuf,int data_len)
{
	hlog("from user %s\n",pbuf);
	pbuf+=CLIENT_NAME_LEN;
	hlog("to user %s\n",pbuf);
	pbuf+=CLIENT_NAME_LEN;
	hlog("receive msg %s\n",pbuf);
}


void rau_app_send_data_to_uart( char * ctrol_data)
{

}

void rau_app_send_data_to_eth(void)
{

}


void rt_rau_app_send_msg_entry(void *parameter)
{
    
}

void rt_rau_app_receive_msg_entry(void *parameter)
{
    char *recv_data;
    int bytes_received;


    recv_data = rt_malloc(BUFSZ);

reconnect:    
    rau_app_tcp_connect_server();
    while(1)
    {

        bytes_received = recv(RauSock, recv_data, BUFSZ - 1, 0);
        if (bytes_received <= 0)
        {

            lwip_close(RauSock);

            rt_free(recv_data);
            break;
        }
        else
        {
            recv_data[bytes_received] = '\0';
            if(rau_app_analysis_data(RauSock,recv_data,bytes_received) == CMD_GET_MEMBER_LIST)
            {
                break;
            }
        }
    }
    while(1)
    {
        char com_rbuf[MAX_LOG_LEN]={0};
        LClock_t StartClock;
        int rlen = 0; 
        rt_memset(com_rbuf,0,MAX_LOG_LEN);
        rt_memset(recv_data,0,BUFSZ);
         rlen = rcu_uart_read(com_rbuf, MAX_LOG_LEN);
        hlog("com data:%s\n",com_rbuf);
        StartClock=GetClock();
        hlog("StartClock:%d\n",StartClock);
        client_send_message(RauSock,member_list[0].client_name,com_rbuf);
        
        bytes_received = recv(RauSock, recv_data, BUFSZ - 1, 0);
        if (bytes_received <= 0)
        {

            lwip_close(RauSock);

            rt_free(recv_data);
            break;
        }
        else
        {
            recv_data[bytes_received] = '\0';
            if(rau_app_analysis_data(RauSock,recv_data,bytes_received) == CMD_GET_MEMBER_LIST)
            {
                break;
            }
        }
    }
    return;
}


int rt_rau_application_init(void) 
{
    rt_thread_t tid;

 //   rt_scheduler_sethook(rt_thread_switch_hook);
/*
    tid = rt_thread_create("rau_send",
                           rt_rau_app_send_msg_entry, RT_NULL,
                           2048, RT_THREAD_RAUAPP_PRIORITY, 20);

    if (tid != RT_NULL)
        rt_thread_startup(tid);
*/
#ifdef RCU_UART
    Clock_Init();
    rcu_uart_init();
    rcu_uart_set_device("uart1");
#endif //RCU_UART

    tid = rt_thread_create("rau_receive",
                           rt_rau_app_receive_msg_entry, RT_NULL,
                           2048, RT_THREAD_RAUAPP_PRIORITY, 20);
    
    if (tid != RT_NULL)
        rt_thread_startup(tid);
    
    return 0;
}


#ifdef RCU_UART
int rcu_uart_init(void)
{
	int ret=0;

	rt_sem_init(&(rcuReadSem), "rcurx", 0, 0);

	return ret;
}
 

static rt_err_t rcu_rx_ind(rt_device_t dev, rt_size_t size)
{
    /* release semaphore to let finsh thread rx data */
    rt_sem_release(&rcuReadSem);

    return RT_EOK;
}


void rcu_uart_set_device(char* uartStr)
{
    static int isInited=0;
    rt_device_t dev = RT_NULL;
                                                        
    if(isInited==1)
        return;

    dev = rt_device_find(uartStr);
    if (dev == RT_NULL)
    {
        hclog("finsh: can not find device: %s\n", uartStr);
        return;
    }
    if(dev == rcuDevice)
        return;

    /* open this device and set the new device */
    if (rt_device_open(dev, RT_DEVICE_OFLAG_RDWR) == RT_EOK)
    {
        if (rcuDevice != RT_NULL)
        {
            /* close old  device */
            rt_device_close(rcuDevice);
            rt_device_set_rx_indicate(dev, RT_NULL);
        }
        rcuDevice = dev;
        rt_device_set_rx_indicate(dev, rcu_rx_ind);
    }
    isInited = 1;
                        
}

int rcu_uart_read(char* buf, int bufLen)
{
    int ret = 0,rlen=0;
    int flag=0;
    char ch=0;
    char* p = buf;
    if(p == NULL)
    {
        return -1;
    }
    while(1)
    {
        if (rt_sem_take(&rcuReadSem, RT_WAITING_FOREVER) != RT_EOK)
        return ret;

        while(rt_device_read(rcuDevice, 0, &ch, 1)==1)
        {
            hclog("rcu_uart_read()--ch:0x%02x \n",ch);

            if(ch == '\r')
            {
                flag = 1;
                //break;
            } 
            else if(flag==1)
            {
                if(ch=='\n')
                {
                    rlen--;
                    flag=2;
                    break;
                }
                else
                {
                    flag=0;
                }   
            }

            if(rlen<bufLen-1)
            {
                *p = ch;
                p++;
            }
            rlen++;
            //if(ch == 0XCE)
            //{
            //	flag = 2;
            //	break;
            //} 

        }
        if(flag==2)
            break; 
    }
    ret = rlen;
    if(rlen<bufLen)
        buf[rlen]=0;
    return ret;
}
int rcu_uart_write(char* buf, int bufLen)
{
	int ret=0;
	rt_uint16_t old_flag = rcuDevice->flag;
	if(rcuDevice==NULL)
		return ret;
	rcuDevice->flag |= RT_DEVICE_FLAG_STREAM;
	ret = rt_device_write(rcuDevice, 0,buf,bufLen);
	rcuDevice->flag = old_flag;
	return ret;
}


void hdp_client_log(const char *fmt, ...)
{
    va_list args;
    rt_size_t length;
    static char rt_log_buf[MAX_LOG_LEN]={0};

    rt_uint16_t old_flag = rcuDevice->flag;

    if(rcuDevice==NULL)
        return;

    va_start(args, fmt);
    /* the return value of vsnprintf is the number of bytes that would be
    * written to buffer had if the size of the buffer been sufficiently
    * large excluding the terminating null byte. If the output string
    * would be larger than the rt_log_buf, we have to adjust the output
    * length. */
    length = rt_vsnprintf(rt_log_buf, sizeof(rt_log_buf) - 1, fmt, args);
    if (length > MAX_LOG_LEN - 1)
        length = MAX_LOG_LEN - 1;       

    rcuDevice->flag |= RT_DEVICE_FLAG_STREAM;
    rt_device_write(rcuDevice, 0, rt_log_buf, length);
    rcuDevice->flag = old_flag;

    va_end(args);
}

#endif //RCU_UART


