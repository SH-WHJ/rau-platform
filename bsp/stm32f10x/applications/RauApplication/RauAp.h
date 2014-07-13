#ifndef __RAUAP_H__
#define __RAUAP_H__
#include <rtconfig.h>

#ifdef __cplusplus
extern "C" {
#endif
#define RT_THREAD_RAUAPP_PRIORITY       0X1c

#define RT_RAU_APP_DEBUG        1

#define CLIENT_NAME_LEN			16
#define CLIENT_PASSWORD_LEN		16
#define PROTOCOL_HEAD_LEN			13

/*–≠“È√¸¡Ó◊÷*/
#define CMD_LOGIN										        0x0001		
#define CMD_LOGOUT 											 0x0002		
#define CMD_GET_MEMBER_LIST 						                0x0003		
#define CMD_SEND_MESSAGE 								         0x0004		
#define CMD_FORWARD_MESSAGE                                                         0x0005		
#define CMD_UPDATE_MEMBER_STATUS                                               0x0006 
#define CMD_RESPONSE 										  0x0100 
#define PROTOCOL_VER_NUB                                                                0x0001


#define   HEAD_ERROR                                                                                -0x0001





#define SEND_BUF_LEN			100
#define RECV_BUG_LEN			100

#define MAX_MEMBER_NUM		10
#define CLIENT_NAME		                "cusaproj10002"
#define CLIENT_PASSWORD		"111111"

#define SOCKET_ERROR    -1


#define SOCKET              int

struct member_list_t 
{
	char client_name[CLIENT_NAME_LEN];
	int  client_state;
};


int rt_rau_application_init(void) ;
void rt_rau_app_receive_msg_entry(void *parameter);
void rt_rau_app_send_msg_entry(void *parameter);
int rau_app_analysis_data(int client,char *pbuf,int len);
void rau_app_tcp_connect_server(void);
void rau_app_send_data_to_uart( char * ctrol_data);
void rau_app_send_data_to_eth(void);
short make_int16(char* ptr);
short make_int32(char* ptr);
void make_net16(char* p, short d);
void make_net32(char* p, int d);
void printf_buf(char *pbuf,int len);
void client_save_member_list(char *pbuf,int data_len);
void client_recvice_message(char *pbuf,int data_len);
int client_get_member_list(SOCKET client);
void rau_app_client_login(SOCKET client);
int client_send_message(SOCKET client,char *p_name,char *p_sting);
int client_send_receive_success(SOCKET client,char *pdata,int cmd_index);
void hdp_client_log(const char *fmt, ...);

int rcu_uart_init();
void rcu_uart_shutdown();
static rt_err_t rcu_rx_ind(rt_device_t dev, rt_size_t size);
void rcu_uart_set_device(char* uartStr);
int rcu_uart_read(char* buf, int bufLen);
int rcu_uart_write(char* buf, int bufLen);

#ifdef __cplusplus
}
#endif    
#endif