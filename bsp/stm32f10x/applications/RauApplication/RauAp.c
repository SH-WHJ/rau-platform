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

#define BUFSZ	1024
#define SERVER_URL      "42.96.202.37"
#define SERVER_PORT   8400      

SOCKET RauSock =0;
struct member_list_t member_list[10];
static unsigned int gclient_cmd_index =0xffffffff;
int g_member_num =0;

//extern void rt_thread_switch_hook(struct rt_thread *from, struct rt_thread *to);

int rau_app_analysis_data(SOCKET client,char *pbuf,int len)
{

    int ret = -1;
    short cmd = 0,cmd_h=0xffff,cmd_l=0xffff, ver = 0;	
    int cmd_index = 0;
    int datalen = 0;	
    char str[100]="this is whj test code";
    if(*pbuf!=0x02)
    {
    	printf("消息头错误\n");
    	return ret;
    }
    pbuf++;
    ver = make_int16(pbuf);
    printf("protocol version number:%d\n",(unsigned int)ver);
    pbuf+=2;

    cmd = make_int16(pbuf);
    printf("command number:0x%02x\n",(unsigned int)cmd);
    cmd_h=cmd&0xff00;
    cmd_l=cmd&0x00ff;
    pbuf+=2;

    cmd_index = make_int32(pbuf);
    printf("command index:%d\n",(unsigned int)cmd_index);
    pbuf+=4;

    datalen = make_int32(pbuf);
    printf("datalen:%d\n",(unsigned int)datalen);
    pbuf+=4;

    if((cmd_l == CMD_LOGIN) && (cmd_h ==CMD_RESPONSE) && (cmd_index==gclient_cmd_index))/*登陆成功*/
    {
    		printf("login success\n");
    		client_get_member_list(client);
    } 
    else if((cmd_l == CMD_GET_MEMBER_LIST) && (cmd_h ==CMD_RESPONSE) && (cmd_index==gclient_cmd_index))/*返回成员列表*/
    {
    		printf("return member\n");
        client_save_member_list(pbuf,datalen);
        //gets(str);
        client_send_message(client,member_list[0].client_name,str);
    }
    else if((cmd_l == CMD_SEND_MESSAGE) && (cmd_h ==CMD_RESPONSE) && (cmd_index==gclient_cmd_index))/*消息发送成功*/
    {
    		printf("send message success\n");
    		//gets(str);
        //client_send_message(client,member_list[0].client_name,str);
    }
    else if((cmd_l == CMD_SEND_MESSAGE) && (cmd_h ==0))
    {
    		printf("receive message\n");
    		printf_buf(pbuf,datalen);
    		client_recvice_message(pbuf,datalen);
    }
    else
    {
    	printf("unknow cmd number received:%d",(unsigned int)cmd);
    }

    return ret;
    
}

void rau_app_tcp_connect_server(void)
{
    struct hostent *host;
    struct sockaddr_in server_addr;

    host = gethostbyname(SERVER_URL);
    while(1)
    {
        if ((RauSock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {

            rt_kprintf("RauSocket create error\n");
            return;
        }


        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SERVER_PORT);
        server_addr.sin_addr = *((struct in_addr *)host->h_addr);
        rt_memset(&(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));

        if (connect(RauSock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
        {
            rt_kprintf("Connect fail!\n");
            lwip_close(RauSock);
            rt_thread_delay(100);        				
            continue;
        }
        break;
    }
    rt_kprintf("联接成功开始登陆请求\n");     
    rau_app_client_login(RauSock);
    
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
		rt_kprintf("0x%02x ",*pbuf);	
		pbuf++;
	}
	rt_kprintf("\n");	
}

/***************************************************************************
*发送客户端登陆请求
***************************************************************************/
void client_fill_in_head(short cmd)
{
	char *pbuf = NULL;
	int len =0;
	
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
    printf_buf(send_buf,data_len);
    err = send(client,send_buf,data_len,0);
    if(err == SOCKET_ERROR)
    {
    	rt_kprintf("send 失败\n");	
    }
    else
    {
    	rt_kprintf("send 成功\n");
    	
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
		rt_kprintf("member:%s \n",pbuf);	
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
	printf_buf(send_buf,data_len);
	err = send(client,send_buf,data_len,0);
	return err; 
}

/***************************************************************************
*显示接收消息内存
***************************************************************************/
void client_recvice_message(char *pbuf,int data_len)
{
	rt_kprintf("from user %s\n",pbuf);
	pbuf+=CLIENT_NAME_LEN;
	rt_kprintf("to user %s\n",pbuf);
	pbuf+=CLIENT_NAME_LEN;
	rt_kprintf("receive msg %s\n",pbuf);
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

        recv_data[bytes_received] = '\0';

        if (strcmp(recv_data , "q") == 0 || strcmp(recv_data , "Q") == 0)
        {
            lwip_close(RauSock);

            rt_free(recv_data);
            break;
        }
        else
        {
            rau_app_analysis_data(RauSock,recv_data,bytes_received);
            //rt_kprintf("\nRecieved data = %s " , recv_data);
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
    
    tid = rt_thread_create("rau_receive",
                           rt_rau_app_receive_msg_entry, RT_NULL,
                           2048, RT_THREAD_RAUAPP_PRIORITY, 20);
    
    if (tid != RT_NULL)
        rt_thread_startup(tid);
    
    return 0;
}

