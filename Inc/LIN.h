/*
 * LIN.h
 *
 *  Created on: 24 сент. 2020 г.
 *      Author: Arh
 */

#ifndef LIN_H_
#define LIN_H_


#include <stdint.h>
#include "GVRET.h"


typedef enum
{
	CRC_CLASSIC,
	CRC_ENHANCED
} crc_type_t;

typedef struct
{
	uint8_t pid; // if pid == 0 empty item
	uint8_t buf[9];
	uint8_t len;
} lin_buf_item_t;

void lin_msg_received(void);
void lin_break_detect(void);
void lin_idle_detect(void);
void lin_fault_detect(void);
void lin_byte_received(uint8_t byte);
void lin_mes_generate(can_msg_t * msg, crc_type_t type);
uint8_t lin_calc_pid(uint8_t id);
uint8_t lin_calc_checksumm(uint8_t * msg, uint8_t len, crc_type_t type);
HAL_StatusTypeDef lin_add_slave_msg(uint8_t pid, uint8_t * buf, uint8_t len);
void lin_send_master_request(uint8_t pid, uint8_t * buf, uint8_t len);
uint8_t lin_calc_checksumm(uint8_t * msg, uint8_t len, uint8_t begin_value);
uint8_t lin_calc_pid(uint8_t id);

#endif /* LIN_H_ */
