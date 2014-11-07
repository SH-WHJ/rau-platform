
#include <rtthread.h>
#include "lwip/def.h"
#include "clientDataProc.h"



static int msg_index=0;

typedef unsigned short u_short;
typedef unsigned int u_int;

#define PROTOCOL_VER 1
#define CLIENG_NAME "TESTCLIENT1"


#define DEVICE_ID 100009
#define DEVICE_TYPE 1
//#define PROTOCOL_VER 0x0001

#define MSGHEADFLAG_BYTES 1  //msg head flag len, char
#define MSGLEN_BYTES 4   //msg len, int
#define MSVER_BYTES 2    //protocol version, short
#define MSGID_BYTES 2    //cmd id, short
#define MSGIDX_BYTES 4   //msg index , int
#define MSGFLAG_BYTES 1  //flag , char
#define MSGDTYPE_BYTES 4 //device type, char
#define MSGDNAMELEN_BYTES 1 //device name len, int
#define MSGDEVID_BYRES 4   //device ID, int
#define MSGPLCID_BYRES 4   //plc ID, int

#define MSGLEN_NOT_INCLUDE (MSGLEN_BYTES+MSGHEADFLAG_BYTES)

#define MSG_ACK_CON_LEN (1+MSGLEN_BYTES+MSVER_BYTES+MSGID_BYTES+MSGIDX_BYTES+MSGFLAG_BYTES+MSGID_BYTES)
#define MSG_HEAD_LEN (1+MSGLEN_BYTES+MSVER_BYTES+MSGID_BYTES+MSGIDX_BYTES+MSGFLAG_BYTES)

#define MSG_REG_CON_LEN (1+MSGLEN_BYTES+MSVER_BYTES+MSGID_BYTES+MSGIDX_BYTES+MSGFLAG_BYTES+MSGDTYPE_BYTES+MSGDNAMELEN_BYTES+MSGDEVID_BYRES+MSGPLCID_BYRES)


enum MSG_ID
{
    MSG_ID_NONE = 0,
    
    MSG_CMD_BEGIN = 1001,
    MSG_CMD_DEVICE_REGISTER, //1002
    MSG_CMD_TRANS_DATA, //1003
    MSG_CMD_SEND_DATA, //1004
	MSG_CMD_GET_DEVLIST, //1005
    MSG_CMD_END = 10000,

    MSG_CTRL_BEGIN = 10001,
    MSG_CMD_DATAFLOW_CTRL, //10002
    MSG_CTRL_END = 20000,

    MSG_INFO_BEGIN = 20001,
    MSG_INFO_UPDATE_STATUS, //20002
    MSG_INFO_UPDATE_CLILIST, //20003
    MSG_INFO_END = 30000,

    MSG_NOTIFY_BEGIN = 30001,
    MSG_NOTIFY_END = 40000,

    MSG_ACK = 50000,

    MSG_ERR = 50001,

    MSG_ID_END
};

typedef enum
{
	DATA_TYPE_STRING = 0,
	DATA_TYPE_DATA, // 1

	DATA_TYPE_MAX = 255,
}DATA_TYPE_ID;



static u_int m_msgindex;


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

int getSendMsgData(int destid, char* data, int dataLen,send_data_t* sptr)
{
	int msgHeadLen = 1+4+2+2+4+1;

	int len = dataLen+msgHeadLen+4+4+1+4;
	int buf_len = len;
	char* p = NULL;

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
	make_net32(p,buf_len-5);
	p+=4;
	make_net16(p,(short)PROTOCOL_VER); //protocol version
	p+=2;
	make_net16(p,(short)MSG_CMD_SEND_DATA); //cmd id
	p+=2;
	
	make_net32(p,m_msgindex++);  //msg index
	p+=4;

	*p = 0;  //flag
	p++;

	make_net32(p,DEVICE_ID);  //msg src id
	p+=4;

	make_net32(p,destid);  //msg dest id
	p+=4;

	*p = (char)DATA_TYPE_DATA;  //data type
	p++;

	make_net32(p,dataLen);  //msg data len
	p+=4;

	memcpy(p,data,dataLen);

	return len;

/*	int len=0;
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
*/	
}

int getRegMsgData(send_data_t* sptr,int plc_id)
{
	int len=0;
	int name_len = strlen(CLIENG_NAME);
	char* p=NULL;
	if(sptr==NULL)
		return -1;
	len = MSG_REG_CON_LEN+name_len;
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
	make_net32(p,(len-MSGLEN_BYTES-1));
	p+=MSGLEN_BYTES;
	make_net16(p,(short)PROTOCOL_VER); //protocol version
	p+=2;
	make_net16(p,(short)MSG_CMD_DEVICE_REGISTER); //cmd id
	p+=2;
	
	make_net32(p,m_msgindex++);  //msg index
	p+=4;

	*p = 0;  //flag
	p++;

	make_net32(p,DEVICE_TYPE);  //device type DEVICE_TYPE
	p+=4;

	make_net32(p,DEVICE_ID);  //device id
	p+=4;	
//	hlog("register to server , device id:%d ",DEVICE_ID);

	make_net32(p,plc_id);  //plc id
	p+=4;

	*p = (char)name_len;  //name len
	p++;

	strcpy(p,CLIENG_NAME);
/*	len = 1+2+2+4+4+MAX_NAME_LEN+MAX_PWD_LEN+1;

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
*/
	return len;
}
int getAckMsgData(short cmd,int index,send_data_t* sptr)
{
	int len=0;
	char* p=NULL;
	len = 1+4+2+2+4+1+2+4;

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
	
	make_net32(p,len-5);
	p+=MSGLEN_BYTES;
	make_net16(p,(short)PROTOCOL_VER); //protocol version
	p+=2;
	make_net16(p,(short)MSG_ACK); //cmd id
	p+=2;
	
	make_net32(p,index);  //msg index
	p+=4;

	*p = 0;  //flag
	p++;

	make_net16(p,(short)cmd); //cmd id
	p+=2;

	make_net32(p,0);
	p+=4;
/*	len = 1+2+2+4+4+11+1;
	
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
*/
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
	int dlen,index,srcid,destid,dataLen;
	char from[MAX_NAME_LEN]={0};
	char to[MAX_NAME_LEN]={0};
	char rdata[128]={0};
	char flag,dataType;
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
	dlen = make_int32(p);
	p+=4;
	p+=2;//version
	cmd = make_int16(p);//ntohs((u_short)*p);
	p+=2;
	index = make_int32(p);//ntohl((u_int)*p);
	p+=4;
	flag = *p;
	p++;

	if(cmd == MSG_CMD_SEND_DATA)
	{
		srcid = make_int32(p);
		p+=4;
		destid = make_int32(p);
		p+=4;
		dataType = *p;
		p++;
		dataLen = make_int32(p);
		p+=4;
		if(DEVICE_ID == destid)
		{
			//send to comm port and wait response
/*			rcu_uart_write(p,dataLen);
			dlen=rcu_uart_read(rdata,128);
			if(dlen<128)
			{
				p = rdata;
				return getSendMsgData(srcid,rdata,dlen,sptr);
			}*/
			sprintf(rdata,"got %s",p);
                    dlen = strlen(rdata);
			return getSendMsgData(srcid,rdata,dlen,sptr);
		}
		
	}


#if 0
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
#endif
	return ret;
}

