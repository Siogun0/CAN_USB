/*
 * LIN.c
 *
 *  Created on: 24 сент. 2020 г.
 *      Author: Arh
 */


#include "LIN.h"


typedef struct
{
	uint8_t byte_cnt;
	uint8_t pid;
	uint8_t data[9];
	uint8_t check_sum;
	uint8_t break_detected;
} lin_stat_t;

typedef struct
{
	uint8_t pid;
	uint8_t len;
	uint8_t data[8];
	uint8_t check_sum;
}lin_msg_t;

typedef enum
{
	CRC_CLASSIC,
	CRC_ENHANCED
} crc_type_t;


lin_stat_t lin_stat;
lin_msg_t lin_msg;

void lin_msg_received(void)
{
	lin_msg.len = lin_stat.byte_cnt - 2;
	lin_msg.pid = lin_stat.pid;
	for(int i = 0; i < lin_msg.len; i++)
		lin_msg.data[i] = lin_stat.data[i];
	lin_msg.check_sum = lin_stat.data[lin_msg.len];

}

void lin_break_detect(void)
{
	if(lin_stat.byte_cnt > 0)
		lin_msg_received();
	lin_stat.byte_cnt = 0;
	lin_stat.break_detected = 1;
}

void lin_idle_detect(void)
{
	if(lin_stat.byte_cnt > 0)
		lin_msg_received();
	lin_stat.break_detected = 0;
}

void lin_byte_received(uint8_t byte)
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
		lin_stat.data[lin_stat.byte_cnt-2] = byte;
		lin_stat.byte_cnt++;
		lin_msg_received();
		break;
	}
}

void lin_mes_generate(lin_msg_t * msg, crc_type_t type)
{
	uint8_t p0, p1;
	uint16_t crc;
	p0 = ((msg->pid >> 0 ^  msg->pid >> 1 ^  msg->pid >> 2 ^  msg->pid >> 4) & 0x01) << 6;
	p1 = (~(msg->pid >> 1 ^  msg->pid >> 3 ^  msg->pid >> 4 ^  msg->pid >> 5) & 0x01) << 7;
	msg->pid &= 0x3F;
	msg->pid |= p0 + p1;

	crc = (type == CRC_CLASSIC) ? 0 : msg->pid;
	for(int i = 0; i < msg->len; i++)
	{
		crc += msg->data[i];
//		crc += crc >> 8;
//		crc &= 0x00FF;
		if(crc >= 256)
			crc -= 255;
	}
	msg->check_sum = ~crc;
}
