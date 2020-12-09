/*
 * LIN.c
 *
 *  Created on: 24 сент. 2020 г.
 *      Author: Arh
 */


#include "LIN.h"

#define SEND_LIN(huart, buf, len) HAL_UART_Transmit_DMA(huart, buf, len)
#define DISABLE_SLAVE
#define ENABLE_SLAVE

typedef struct
{
	uint8_t byte_cnt;
	uint8_t pid;
	uint8_t data[9];
	uint8_t check_sum;
	uint8_t break_detected;
	boolean transmiting;
} lin_stat_t;

typedef struct
{
	uint8_t pid;
	uint8_t len;
	uint8_t data[8];
	uint8_t check_sum;
}lin_msg_t;

#define LIN_SLAVE_BUF_LEN	32
lin_buf_item_t lin_slave_buf[LIN_SLAVE_BUF_LEN];



lin_stat_t lin_stat;
lin_msg_t lin_msg;
uint8_t lin_send_buffer[11];

extern UART_HandleTypeDef huart1;

boolean lin_check_slave_answer(uint8_t pid);

uint8_t lin_calc_pid(uint8_t id)
{
	uint8_t p0, p1, pid;
	p0 = ((id >> 0 ^ id >> 1 ^ id >> 2 ^ id >> 4) & 0x01) << 6;
	p1 = (~(id >> 1 ^ id >> 3 ^ id >> 4 ^ id >> 5) & 0x01) << 7;
	pid = (id & 0x3F) | (p0 + p1);
	return pid;
}

uint8_t lin_calc_checksumm(uint8_t * msg, uint8_t len, uint8_t begin_value)
{
	uint16_t crc;
	crc = begin_value;
	for(int i = 0; i < len; i++)
	{
		crc += msg[i];
//		crc += crc >> 8;
//		crc &= 0x00FF;
		if(crc >= 256)
			crc -= 255;
	}
	return ~crc;
}


can_msg_t lin_msg_parse(void)
{
	can_msg_t msg = {0};
	uint8_t checksumm;
	msg.timestamp = HAL_GetTick();
	msg.header.DLC = lin_stat.byte_cnt-3;
	msg.data_byte[0] = lin_stat.data[0];
	msg.data_byte[1] = lin_stat.data[1];
	msg.data_byte[2] = lin_stat.data[2];
	msg.data_byte[3] = lin_stat.data[3];
	msg.data_byte[4] = lin_stat.data[4];
	msg.data_byte[5] = lin_stat.data[5];
	msg.data_byte[6] = lin_stat.data[6];
	msg.data_byte[7] = lin_stat.data[7];
	msg.lin_checksumm = lin_stat.data[msg.header.DLC];
	msg.can_dir = (lin_stat.transmiting) ? DIR_TRANSMIT : DIR_RECEIVE;
	msg.bus = 3;

	checksumm = lin_calc_checksumm(msg.data_byte, msg.header.DLC, lin_stat.pid);
	if(checksumm + msg.lin_checksumm == 0xFF)
	{
		msg.header.ExtId = lin_stat.pid;
		msg.header.IDE = CAN_ID_EXT;
	}
	else
	{
		msg.header.StdId = lin_stat.pid;
		msg.header.IDE = CAN_ID_STD;
	}
	return msg;

}

void lin_msg_received(void)
{
	can_msg_t temp_msg;
	temp_msg = lin_msg_parse();
	CAN_Buffer_Write_Data(temp_msg);
	CAN_Log_Buffer_Write_Data(temp_msg);
}

void lin_break_detect(void)
{
	if(lin_stat.byte_cnt > 0)
		lin_msg_received();
	lin_stat.byte_cnt = 0;
	lin_stat.break_detected = 1;
	lin_stat.data[0] = lin_stat.data[1] = lin_stat.data[2] = lin_stat.data[3] = lin_stat.data[4] = lin_stat.data[5] = lin_stat.data[6] = lin_stat.data[7] = lin_stat.data[8] = 0;
	lin_stat.pid = 0;
}

void lin_idle_detect(void)
{
	if(lin_stat.byte_cnt > 0)
		lin_msg_received();
	lin_stat.break_detected = 0;
}

void lin_fault_detect(void)
{
	lin_stat.break_detected = 0;
	lin_stat.byte_cnt = 0;
}

void lin_byte_received(uint8_t byte)
{
	if(lin_stat.break_detected)
	{
		switch(lin_stat.byte_cnt)
		{
		case 0:
			if(byte == 0x55)
			{
				lin_stat.byte_cnt++;
			}
			else
			{
				lin_stat.break_detected = 0;
			}
			break;
		case 1:
			lin_stat.pid = byte;
			lin_stat.byte_cnt++;
			lin_stat.transmiting = lin_check_slave_answer(lin_stat.pid);
			break;
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
			lin_stat.data[lin_stat.byte_cnt-2] = byte;
			lin_stat.byte_cnt++;
			break;
		case 10:
			lin_stat.data[8] = byte;
			lin_stat.byte_cnt++;
			lin_msg_received();
			break;
		}
	}

}

void lin_add_slave_msg(uint8_t pid, uint8_t * buf, uint8_t len)
{
	for(uint8_t i = 0; i < LIN_SLAVE_BUF_LEN; i++)
	{
		if(lin_slave_buf[i].pid == 0 || lin_slave_buf[i].pid == pid)
		{
			lin_slave_buf[i].pid = pid;
			lin_slave_buf[i].len = len;
			lin_slave_buf[i].buf[0] = buf[0];
			lin_slave_buf[i].buf[1] = buf[1];
			lin_slave_buf[i].buf[2] = buf[2];
			lin_slave_buf[i].buf[3] = buf[3];
			lin_slave_buf[i].buf[4] = buf[4];
			lin_slave_buf[i].buf[5] = buf[5];
			lin_slave_buf[i].buf[6] = buf[6];
			lin_slave_buf[i].buf[7] = buf[7];
			lin_slave_buf[i].buf[8] = buf[8];
			break;
		}
	}
}

boolean lin_check_slave_answer(uint8_t pid)
{
	for(uint8_t i = 0; i < LIN_SLAVE_BUF_LEN; i++)
	{
		if(lin_slave_buf[i].pid == pid)
		{
			DISABLE_SLAVE;
			SEND_LIN(&huart1, lin_slave_buf[i].buf, lin_slave_buf[i].len);
			lin_slave_buf[i].pid = 0;
			return true;
		}
	}
	return false;
}

void lin_send_master_request(uint8_t pid, uint8_t * buf, uint8_t len)
{
	lin_send_buffer[0] = 0x55;
	lin_send_buffer[1] = lin_calc_pid(pid);
	for(uint8_t i = 0; i < len; i++)
	{
		lin_send_buffer[i+2] = buf[i];
	}
	USART1->CR1 |= USART_CR1_SBK;
	while(USART1->CR1 & USART_CR1_SBK);
	SEND_LIN(&huart1, lin_send_buffer, len+2);
}
