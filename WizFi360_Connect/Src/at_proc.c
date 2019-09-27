/*
 * at_proc.c
 *
 *  Created on: Sep 5, 2019
 *      Author: Teddy
 */

#include "at_proc.h"
#include "string.h"
#include "stdlib.h"
#include "stm32f1xx_hal_uart.h"

struct queue WizFi_Queue;
struct us_data u2_data;
uint8_t WIFI_SSID[50]="Teddy_AP";
uint8_t WIFI_PW[50]="12345678";
void Set_WIFI_DATA(uint8_t *SSID, uint8_t *PW)
{
	memcpy(WIFI_SSID, SSID, strlen((char*)SSID));
	memcpy(WIFI_PW, PW, strlen((char*)PW));
}
#if 1	//temp data test uart2
void Get_WIFI_DATA(uint8_t *SSID, uint8_t *PW)
{
	memcpy(SSID, WIFI_SSID, strlen((char*)WIFI_SSID));
	memcpy(PW, WIFI_PW, strlen((char*)WIFI_PW));
}

void init_U2_data(void)
{
	memset(&u2_data, 0, sizeof(u2_data));
}
void input_U2_data(uint8_t data)
{
	u2_data.RX_Buffer[u2_data.index++] = data;
	if(data =='\r')
		u2_data.flag = 1;
}
uint8_t U2_flag(void)
{
	uint8_t u2_data_flag = u2_data.flag;
	if(u2_data_flag)
	{
		u2_data.flag = 0;
		u2_data.RX_Buffer[u2_data.index] = 0;
		printf("u2[%d]%s\r\n",u2_data.index, u2_data.RX_Buffer);
		data_Proc(1, 0, u2_data.index, u2_data.RX_Buffer);
		u2_data.index = 0;
	}
	else
	{
		data_Proc(0, 0, 0, 0);
	}
	
	return u2_data_flag;
}
#endif
//queue function
void Init_Queue(void)
{
	memset(&WizFi_Queue, 0, sizeof(WizFi_Queue));
}
uint8_t Queue_Full(void)
{
	if(WizFi_Queue.head + 1 >= MAX_BUFF)
	{
		if(WizFi_Queue.tail == 0)
			return 1;
	}
	else
	{
		if((WizFi_Queue.head + 1) == WizFi_Queue.tail)
			return 1;
	}
	return 0;
}
uint16_t Queue_Empty(void)
{
	if(WizFi_Queue.head >= WizFi_Queue.tail)
		return WizFi_Queue.head - WizFi_Queue.tail;
	else
		return MAX_BUFF - WizFi_Queue.tail + WizFi_Queue.head;
}
void EnQueue(uint8_t input)
{
	if(WizFi_Queue.head + 1 >= MAX_BUFF)
		WizFi_Queue.head = 0;

	if(Queue_Full())
		return;

	WizFi_Queue.data[WizFi_Queue.head++] = input;
}
uint8_t DeQueue(void)
{
	if(WizFi_Queue.tail + 1 >= MAX_BUFF)
		WizFi_Queue.tail = 0;

	if(Queue_Empty() == 0)
		return 0xFF;

	return WizFi_Queue.data[WizFi_Queue.tail++];
}


uint8_t delay_count(uint16_t *time1, uint16_t *time2, uint16_t set_time)
{
	*time1 += 1;
	if(*time1 > 60000)
	{
		//delay count action
		*time2 += 1;
		*time1 = 0;
		if(*time2 > set_time)
		{
			*time2 = 0;
			return 1;
		}
	}
	return 0;
}
uint8_t match_ok(uint8_t *data, uint16_t len)
{
	uint16_t cnt = 0;
	for(cnt=0; cnt < len; cnt++)
	{
		if((data[cnt]=='O') && (data[cnt+1] == 'K'))
		{
				return 1;
		}
	}
	return 0;
}
int AT_CMD_send(uint8_t *cmd, enum cmd_send_type type, uint8_t sock, uint16_t val, uint8_t *S_data)
{
	int len = 0;
	uint8_t cmd_buff[256];
	if(type == none)
	{
		len = sprintf((char*)cmd_buff,"AT+%s=%d\r\n", cmd, val);
	}
	else if(type == noneval)
	{
		len = sprintf((char*)cmd_buff,"AT+%s\r\n", cmd);
	}
	else if(type == CUR_int)
	{
		len = sprintf((char*)cmd_buff,"AT+%s_CUR=%d\r\n", cmd, val);
	}
	else if(type == DEF_int)
	{
		len = sprintf((char*)cmd_buff,"AT+%s_DEF=%d\r\n", cmd, val);
	}
	else if(type == CUR_str)
	{
		len = sprintf((char*)cmd_buff,"AT+%s_CUR=%s\r\n", cmd, S_data);
	}
	else if(type == DEF_str)
	{
		len = sprintf((char*)cmd_buff,"AT+%s_DEF=%s\r\n", cmd, S_data);
	}
	else if(type == none_str)
	{
		len = sprintf((char*)cmd_buff,"AT+%s=%s\r\n", cmd, S_data);
	}
	else
	{
		len = sprintf((char*)cmd_buff,"AT+%s=%d,%d\r\n", cmd, sock, val);
	}
	printf("send[%d]%s\r\n", len, cmd_buff);
	send_U_message(cmd_buff, len);
	return len;
}
int AT_CMD_Proc(uint8_t *cmd, enum cmd_send_type type, uint8_t sock, uint16_t val, uint8_t *S_data, uint8_t *re_data, uint16_t time, FuncPtr func)
{
	static uint8_t req = 0, retry = 0, pre_len = 0;
	static uint16_t Proc_cnt = 0, Proc_cnt1 = 0;
	uint8_t temp_buf[512];
	uint16_t temp_index = 0, data_len = 0;
	if(req == 0)
	{
		AT_CMD_send(cmd, type, sock, val, S_data);
		req = 1;
		Proc_cnt = 0;
		Proc_cnt1 = 0;
	}
	else if(req == 1)
	{
		if(delay_count(&Proc_cnt, &Proc_cnt1, 3))
			req = 2;
	}
	else
	{
		data_len = Queue_Empty();
		if((data_len > 0) &&(data_len == pre_len))
		{
			while(Queue_Empty())
			{
				temp_buf[temp_index++] = DeQueue();
			}
			temp_buf[temp_index] = 0;
			printf("recv[%d]%s \r\n", temp_index, temp_buf);
			if(func(temp_buf, strlen(temp_buf)))
			{
				req = 0;
				Proc_cnt = 0;
				Proc_cnt1 = 0;
				return 1;
			}
			else
			{
				//error action
				retry++;
			}
		}
		else
		{
			pre_len = data_len;
		}
		if(delay_count(&Proc_cnt, &Proc_cnt1, time))
		{
			//delay count action
			req = 0;
			retry++;
		}
	}
	return 0;
}
int Recv_Proc(FuncPtr func, uint16_t time)
{
	static uint16_t Proc_cnt = 0, Proc_cnt1 = 0, pre_len = 0;
	uint8_t temp_buf[512];
	uint16_t temp_index = 0, data_len = 0;

	
	if(delay_count(&Proc_cnt, &Proc_cnt1, time))
	{
		//delay count action
		data_len = Queue_Empty();
		if((data_len > 0) &&(data_len == pre_len))
		{
			while(Queue_Empty())
			{
				temp_buf[temp_index++] = DeQueue();
			}
			temp_buf[temp_index] = 0;
			printf("recv[%d]%s \r\n", temp_index, temp_buf);
			if(func(temp_buf, temp_index))
			{
				Proc_cnt = 0;
				Proc_cnt1 = 0;
				return 1;
			}
			else
			{
				//error action
				pre_len = 0;
			}
		}
		else
		{
			pre_len = data_len;
		}
	}
	return 0;
}
int AT_Connect_Proc(void)
{
	static uint8_t seq = 0;
	switch(seq)
	{
	case 0:		//CWMODE_CUR = 1
		if(AT_CMD_Proc("CWMODE", CUR_int, 0, 1, 0, 0, 10, match_ok))
			seq++;
		break;
	case 1:		//CWLAP
		if(AT_CMD_Proc("CWLAP", noneval, 0, 0, 0, 0, 100, match_ok))
			seq++;
		break;
	case 2:		//CWJAP
		if(AT_CMD_Proc("CWJAP", CUR_str, 0, 0, "\"Teddy_AP\",\"12345678\"", 0, 100, match_ok))
			seq++;
		break;
	}
	return 0;
}
int AT_AirKiss_Proc(void)
{
	static uint8_t seq = 0;
	switch(seq)
	{
	case 0:		//CWMODE_CUR = 1
		if(AT_CMD_Proc("CWMODE", CUR_int, 0, 1, 0, 0, 10, match_ok))
			seq++;
		break;
	case 1:		//CWLAP
		if(AT_CMD_Proc("CWSTARTSMART", none, 0, 2, 0, 0, 100, match_ok))
			seq++;
		break;
	case 2:		//CWJAP
		if(Recv_Proc(AirKissConnect, 3))
		{
			printf("AirKiss Success !!\r\n");
			seq++;
		}
		break;
	case 3:
		if(AT_CMD_Proc("CIFSR", noneval, 0, 0, 0, 0, 10, match_ok))
			seq++;
		break;
	case 4:
		if(AT_CMD_Proc("CIPMUX", none, 0, 1, 0, 0, 10, match_ok))
			seq++;
		break;
	case 5:
		if(AT_CMD_Proc("CIPSERVER", none_soc, 1, 5001, 0, 0, 100, match_ok))
			seq++;
		break;
	case 6:
	#if 0
		if(Recv_Proc(RecvDataPars, 3))
		{
			printf("data recieve !!\r\n");
			seq++;
		}
		#endif
		return 1;
		break;
	}
	return 0;
}
int data_Proc(uint8_t mode, uint8_t sock, uint16_t val, uint8_t *S_data)
{
	uint8_t status = 0;
	if(mode)
	{
		//send mode
#if 0
		if(Queue_Empty() > 0)
			return 0;
#endif
		while(status == 0)
		{
			status = AT_CMD_Proc("CIPSEND", none_soc, sock, val, 0, 0, 10, match_ok);
		}
		if(status == 1)
		{
			send_U_message(S_data, val);
			S_data[val] = 0;
			printf("send data[%d]%s\r\n",val, S_data);
			return 1;
		}
		return 0;
	}
	else
	{
		//recv mode
		if(Recv_Proc(RecvDataPars, 3))
		{
			//printf("data recieve !!\r\n");
			return 1;
		}
		return 0;
	}
	return 1;
}

int AirKissConnect(uint8_t *data, uint16_t len)
{
	uint16_t cnt = 0;
	for(cnt=0; cnt < len; cnt++)
	{
		if((data[cnt]=='c') && (data[cnt+1] == 'o'))
		{
			if(memcmp(&data[cnt], "connected wifi", strlen("connected wifi")) == 0)
			{
				//data[len] = 0;
				//printf("Airkiss recv[%d][%s]\r\n",sizeof("connected wifi"), &data[cnt]);
				return 1;
			}
				
		}
	}
	return 0;
}

int RecvDataPars(uint8_t *data, uint16_t len)
{
	uint16_t cnt = 0;
	char *ptr;
	int data1, data2;
	for(cnt=0; cnt < len; cnt++)
	{
		if(memcmp(&data[cnt], "+IPD,", strlen("+IPD,")) == 0)
		{
			cnt = cnt+strlen("+IPD,");
			data[len] = 0;
			ptr = strtok(&data[cnt],",");
			if(ptr == NULL)
			{
				//single
				ptr = strtok((char *)&data[cnt],":");
				data1 = atoi(ptr);
				printf("LEN:%d, str:%s \r\n", data1, ptr);
				return 1;
			}
			else
			{
				//multi 
				data2 = atoi(ptr);
				ptr = strtok(NULL,":");
				data1 = atoi(ptr);
				ptr = strtok(NULL,":");

				printf("socket: %d LEN:%d data:%s \r\n", data2, data1, &data[len - data1]);
				return 1;
			}
			
		}
	}
	return 0;
}
