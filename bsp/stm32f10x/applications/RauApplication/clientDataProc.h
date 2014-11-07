
#ifndef _CLIENT_DATA_PROC_H_
#define _CLIENT_DATA_PROC_H_

#define BUFSZ	1024

#define MAX_NAME_LEN 16
#define MAX_PWD_LEN 16

#define DEVICE_NAME "test11"

#define NULL RT_NULL

//#define DEV_RCU   //read from uart1
#define DEV_RAU


#define hlog hdp_client_log //rt_kprintf

typedef struct
{
	int data_len;
	char* data_ptr;
}send_data_t;

void hdp_client_log(const char *fmt, ...);

int getRegMsgData(send_data_t* sptr,int plc_id);
int client_data_proc(char* data, int len, send_data_t* sptr);
int getSendMsgData(int destid, char* data, int dataLen,send_data_t* sptr);

#endif //_CLIENT_DATA_PROC_H_
