
#include <rtthread.h>
#include "lwip/def.h"
#include "clientDataProc.h"



static int msg_index=0;

typedef unsigned short u_short;
typedef unsigned int u_int;



short make_int16(char* ptr)
{
	short ret = 0;
	short temp = 0;//(short)*ptr;
	rt_memcpy((char*)&temp,(char*)ptr,2);
	ret = (short)ntohs((u_short)temp);
	return ret;
}
int make_int32(char* ptr)
{
	int ret = 0;
	int temp = 0;//(short)*ptr;
	rt_memcpy((char*)&temp,(char*)ptr,4);
	ret = (int)ntohl((u_int)temp);
	return ret;
}
void make_net16(char*p,u_short s)
{
	u_short tmp = htons(s);
	rt_memcpy(p,(char*)&tmp,2);
}
void make_net32(char* p,u_int s)
{
	u_int tmp = htonl(s);
	rt_memcpy(p,(char*)&tmp,4);
}

int getSendMsgData(char* dest, char* data, int dataLen,send_data_t* sptr)
{
	int len=0;
	int dlen=0;
	char* p=NULL;
	if(sptr==NULL || dest==NULL || data==NULL || dataLen<=0)
		return -1;

	dlen = 2*MAX_NAME_LEN+dataLen;
	len = 1+2+2+4+4+dlen+1;

	if((len>sptr->data_len) || sptr->data_ptr==RT_NULL)
	{
		p = (char*)rt_malloc(len);
		if(p==NULL)
		{
			return -1;
		}
		sptr->data_ptr = p;
		sptr->data_len = len;

		rt_memset(p,0,len);
	}
	else 
	{
		p = sptr->data_ptr;
		rt_memset(p,0,sptr->data_len);
	}

	*p = 0x02;
	p++;
	make_net16(p,0x0001);
	p+=2;
	make_net16(p,0x0004);
	p+=2;

	make_net32(p,msg_index++);
	p+=4;

	make_net32(p,dlen);
	p+=4;

	strcpy(p,DEVICE_NAME);
	p+=MAX_NAME_LEN;

	strcpy(p,dest);
	p+=MAX_NAME_LEN;

	memcpy(p,data,dataLen);
	p+=dataLen;
	
	*p = 0x03;

	return len;
	
}

int getRegMsgData(send_data_t* sptr)
{
	int len=0;
	char* p=NULL;
	if(sptr==NULL)
		return -1;
	len = 1+2+2+4+4+MAX_NAME_LEN+MAX_PWD_LEN+1;

	if((len>sptr->data_len) || sptr->data_ptr==RT_NULL)
	{
		p = (char*)rt_malloc(len);
		if(p==NULL)
		{
			return -1;
		}
		sptr->data_ptr = p;
		sptr->data_len = len;

		rt_memset(p,0,len);
	}
	else 
	{
		p = sptr->data_ptr;
		rt_memset(p,0,sptr->data_len);
	}
	
	
	*p = 0x02;
	p++;
	make_net16(p,0x0001);
	p+=2;
	make_net16(p,0x0001);
	p+=2;

	make_net32(p,msg_index++);
	p+=4;

	make_net32(p,32);
	p+=4;

	strcpy(p,DEVICE_NAME);
	p+=32;
	*p = 0x03;

	return len;
}
int getAckMsgData(short cmd,int index,send_data_t* sptr)
{
	int len=0;
	char* p=NULL;
	len = 1+2+2+4+4+11+1;
	
	if((len>sptr->data_len) || sptr->data_ptr==NULL)
	{
		p = (char*)rt_malloc(len);
		if(p==NULL)
		{
			return -1;
		}
		sptr->data_ptr = p;
		sptr->data_len = len;

		rt_memset(p,0,len);
	}
	else 
	{
		p = sptr->data_ptr;
		rt_memset(p,0,sptr->data_len);
	}
	
	*p = 0x02;
	p++;
	make_net16(p,0x0001);
	p+=2;
	make_net16(p,cmd);
	p+=2;

	make_net32(p,index);
	p+=4;

	make_net32(p,11);
	p+=4;

	strcpy(p,"ACT_SUCCESS");
	p+=11;
	*p = 0x03;

	return len;
}
static const char* fdReplyStr="Reply for: ";
int getForwardReply(int index,char* from, char* to,char* msg,int msgLen,send_data_t* sptr)
{
	int len=0,dlen=0;
	char* p=NULL;
	//dlen = 2*MAX_NAME_LEN+strlen(fdReplyStr)+msgLen;
	dlen = 2*MAX_NAME_LEN+msgLen;
	len = 1+2+2+4+4+1+dlen;
	
	if((len>sptr->data_len) || sptr->data_ptr==NULL)
	{
		p = (char*)rt_malloc(len);
		if(p==NULL)
		{
			return -1;
		}
		sptr->data_ptr = p;
		sptr->data_len = len;

		rt_memset(p,0,len);
	}
	else 
	{
		p = sptr->data_ptr;
		rt_memset(p,0,sptr->data_len);
	}
	
	*p = 0x02;
	p++;
	make_net16(p,0x0001);
	p+=2;
	make_net16(p,0x0004);
	p+=2;

	make_net32(p,index);
	p+=4;

	make_net32(p,dlen);
	p+=4;

	strcpy(p,to);
	p+=MAX_NAME_LEN;

	strcpy(p,from);
	p+=MAX_NAME_LEN;

/*	strcpy(p,fdReplyStr);
	p+=strlen(fdReplyStr);
*/
//	strcpy(p,msg);
	rt_memcpy(p,msg,msgLen);
	p+=msgLen;

	*p = 0x03;

	return len;
}

extern int rcu_uart_read(char* buf, int bufLen);
extern int rcu_uart_write(char* buf, int bufLen);
int client_data_proc(char* data, int len, send_data_t* sptr)
{
	int ret=0;
	int mlen=0;
	short cmd;
	int dlen,index;
	char from[MAX_NAME_LEN]={0};
	char to[MAX_NAME_LEN]={0};
	char rdata[128]={0};
	char* p = data;
	if(data==NULL || len<=0)
		return ret;
	hlog("processRecvMsg()--len:%d\n",len);
	if(*p!=0x02)
	{
		hlog("processRecvMsg()--msg not begin with 0x02\n");
		return ret;
	}
	p++;
	p+=2;//version
	cmd = make_int16(p);//ntohs((u_short)*p);
	p+=2;
	index = make_int32(p);//ntohl((u_int)*p);
	p+=4;
	dlen = make_int32(p);//ntohl((u_int)*p);
	p+=4;

	hlog("recv msg,cmd:%d,index:%d,dlen:%d\n",(int)cmd,index,dlen);

	if(cmd == 0x0004)
	{
		strcpy(from,p);
//		hlog("msg from:%s\n",from);
		p+=MAX_NAME_LEN;
		strcpy(to,p);
//		hlog("msg to:%s\n",to);
		p+=MAX_NAME_LEN;
//		hlog("msg content:%s\n",p);
		mlen = dlen-2*MAX_NAME_LEN;
		p[mlen]=0;
#ifdef DEV_RAU
		rcu_uart_write("\nRECV:",6);
		rcu_uart_write(p,mlen);
		rcu_uart_write("\n",1);
		//ret = getAckMsgData(cmd,index,sptr);
		dlen=rcu_uart_read(rdata,128);
		if(dlen<128)
		{
			p = rdata;
		}
		else
		{
			dlen = mlen;
		}
#endif
		ret = getForwardReply(index,from,to,p,dlen,sptr);
	}
	else if(cmd == 0x0104)
	{
		strcpy(from,p);
//		hlog("msg from:%s\n",from);
		p+=MAX_NAME_LEN;
		strcpy(to,p);
//		hlog("msg to:%s\n",to);
		p+=MAX_NAME_LEN;
//		hlog("msg content:%s",p);
		mlen = dlen-2*MAX_NAME_LEN;
		p[mlen]=0;

		//ret = getAckMsgData(cmd,index,sptr);
		ret = 0;
	}
	else if(cmd == 0x0103)
	{
		hlog("recv ack msg.....\n");
/*		char tmp[MAX_NAME_LEN+1]={0};
		TCHAR utmp[2*MAX_NAME_LEN+2]={0};
		m_devlist.ResetContent();

		int num = dlen/(MAX_NAME_LEN+1);
		hlog("get device list num:%d\n",num);
//		p+=4;
		for(int i=0;i<num;i++)
		{
			strcpy_s(tmp,p);
		#ifdef UNICODE  
			MultiByteToWideChar(CP_ACP, 0, tmp, -1, utmp, 2*MAX_NAME_LEN);  
		#else  
			strcpy_s(buf, str);  
		#endif  
			m_devlist.AddString(utmp);
			if(i<(num-1))
				p+=(MAX_NAME_LEN+1);
		}	
*/
	}
	return ret;
}

