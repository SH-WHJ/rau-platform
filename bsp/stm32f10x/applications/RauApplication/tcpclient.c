
#include <rtthread.h>
#include <rtdef.h>
#include <lwip/netdb.h> 
#include <lwip/sockets.h>

#include "clientDataProc.h"

#include "stm32f10x.h"

#include <stdio.h>

//#define UART_TEST

#define RCU_UART 

#ifdef RCU_UART
int rcu_uart_init();
void rcu_uart_shutdown();
static rt_err_t rcu_rx_ind(rt_device_t dev, rt_size_t size);
void rcu_uart_set_device(char* uartStr);
int rcu_uart_read(char* buf, int bufLen);
int rcu_uart_write(char* buf, int bufLen);
#endif

extern void led_on(int i);
extern short make_int16(char* ptr);
//extern int gettimeofday(struct timeval *tp, void *ignore);


//#define hclog hdp_client_log   //rt_kprintf
#define hclog rt_kprintf


static const char* server_url="42.96.202.37";
static const int server_port = 8400;

static char send_buf[BUFSZ]={0};

send_data_t sdata={0};

int spend_time_sec=0;


#ifdef RCU_UART
static struct rt_semaphore rcuReadSem;
static struct rt_device * rcuDevice;
#define MAX_LOG_LEN 128
#endif




void tcpclient(void* parameter)
{
	int slen=0;
	int rlen = 0;
	int index=0;
	char *recv_data;
	char* p;
	short cmd;
	struct hostent *host;
	int sock, bytes_received,bytes_sended;
	struct sockaddr_in server_addr;
#ifndef DEV_RAU
	char com_rbuf[MAX_LOG_LEN]={0};
#endif



#ifdef RCU_UART
	rcu_uart_init();
	rcu_uart_set_device("uart1");
#endif //RCU_UART


	sdata.data_len = BUFSZ;
	sdata.data_ptr = (char*)send_buf;
	
	/* 通过函数入口参数url获得host地址（如果是域名，会做域名解析） */
	host = gethostbyname(server_url);
	
	/* 分配用于存放接收数据的缓冲 */
	recv_data = rt_malloc(BUFSZ);
	if (recv_data == RT_NULL)
	{
		hclog("No memory\n");
		return;
	}
reconnect:
	while(1)
	{

		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)    
		{        
			hclog("Socket error\n");       
			rt_free(recv_data);        
			return;    
		} 
		
		server_addr.sin_family = AF_INET;  
		server_addr.sin_port = htons(server_port);   
		server_addr.sin_addr = *((struct in_addr *)host->h_addr);   
	//	server_addr.sin_addr.s_addr = inet_addr(server_url);
		rt_memset(&(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));    
		
	
		if (connect(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)   
		{        
			bytes_received = errno;
			hclog("Connect fail!,errno:%d,try angin after a while....\n",bytes_received); 
			//break;//test
			lwip_close(sock); 
    
			rt_thread_delay(100);
/*        
		    if(index & 0x00000001)
				led_on(1);
			else
				led_off(1);
			index++;
*/			
		    continue;
		} 
		break;
	}
    hclog("connect to server:%s success\n",server_url);



	//register
	slen = getRegMsgData(&sdata);
	if(slen>0)
	{
		while(1)
		{
			bytes_sended = send(sock,sdata.data_ptr,slen, 0);
			if(sdata.data_ptr!=send_buf)
			{
				rt_free(sdata.data_ptr);
				sdata.data_ptr=NULL;
				sdata.data_len=0;
			}
			if(bytes_sended!=slen)
			{
				hclog("send register data failed!\n");
				//break;//test
				continue;
			}
			bytes_received = recv(sock, recv_data, BUFSZ - 1, 0);
			if(bytes_received==0)
			{
				lwip_close(sock); 
				goto reconnect;
			}
			if(bytes_received>0)
			{
				p = recv_data;
				p+=3;
				cmd = make_int16(p);
				if(cmd==0x0101)
				{
					hclog("register success.....\n");
					break;
				}
			}
		}
		
	}
#ifdef DEV_RAU	
	while(1)    
	{        
		rt_memset(recv_data,0,BUFSZ);
		sdata.data_len = BUFSZ;
		sdata.data_ptr = (char*)send_buf;
	
		bytes_received = recv(sock, recv_data, BUFSZ - 1, 0);       
		if (bytes_received == 0)        
		{            
			
			//lwip_close(sock); 
			
			//rt_free(recv_data);       
			//break;      

			lwip_close(sock); 
			goto reconnect;
		}  
		slen=client_data_proc(recv_data,bytes_received,&sdata);
		
        if(slen>0)
        {
			bytes_sended = send(sock,sdata.data_ptr,slen, 0); 
			if(sdata.data_ptr!=send_buf)
			{
				rt_free(sdata.data_ptr);
				sdata.data_ptr=NULL;
				sdata.data_len=0;
			}
			if(bytes_sended!=slen)
			{
				hclog("send ack data failed!\n");
			}
        }

	} 
#else //DEV_RAU
	while(1)
	{
		rt_memset(com_rbuf,0,MAX_LOG_LEN);
		rt_memset(recv_data,0,BUFSZ);
		sdata.data_len = BUFSZ;
		sdata.data_ptr = (char*)send_buf;

//		rcu_uart_write("\nRCU>>",5);

		rlen = rcu_uart_read(com_rbuf, MAX_LOG_LEN);
		if(rlen>0 && rlen<MAX_LOG_LEN)
		{
//			spend_time_sec = gettimeofday(NULL,NULL);
			hlog("send data:%s\n",com_rbuf);
			slen = getSendMsgData("test11",com_rbuf,rlen,&sdata);
			if(slen>0)
	        {
				bytes_sended = send(sock,sdata.data_ptr,slen, 0); 
				if(sdata.data_ptr!=send_buf)
				{
					rt_free(sdata.data_ptr);
					sdata.data_ptr=NULL;
					sdata.data_len=0;
				}
				if(bytes_sended!=slen)
				{
					hclog("send uart data(%s) failed!\n",com_rbuf);
				}
				else
				{
					bytes_received = recv(sock, recv_data, BUFSZ - 1, 0);
					if(bytes_received==0)
					{
						lwip_close(sock); 
						goto reconnect;
					}
					if(bytes_received>0)
					{
//						spend_time_sec = gettimeofday(NULL,NULL)-spend_time_sec;
						hclog("recv data spend time(seconds):%d\n",spend_time_sec);
						slen=client_data_proc(recv_data,bytes_received,&sdata);
					}
				}
	        }
		}
	}
#endif //DEV_RAU

	return;
}

#ifdef RCU_UART
int rcu_uart_init()
{
	int ret=0;

	rt_sem_init(&(rcuReadSem), "rcurx", 0, 0);

	return ret;
}

void rcu_uart_shutdown()
{

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
        rt_kprintf("finsh: can not find device: %s\n", uartStr);
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


