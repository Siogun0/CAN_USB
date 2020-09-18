/*
 * GVRET.c
 *
 *  Created on: 5 ���. 2020 �.
 *      Author: Arh
 */
#include "GVRET.h"
#include "can.h"
#include <string.h>
#include <math.h>
#include "eeprom.h"
#include "main.h"


// command buffer
uint8_t cmd_buf[CMD_BUFFER_LENGTH];

// command buffer index
uint8_t buf_ind = 0;

conf_t conf;

extern uint8_t uart_tx_bufer[1024];
extern uint8_t uart_tx_com_bufer[32];
extern uint8_t uart_answ_ready;
extern uint32_t uart_tx_pointer;
extern uint8_t uart_rx_bufer[1024];
extern uint8_t uart_rx_char;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef *huart_active;
extern UART_HandleTypeDef *huart_lin;
extern uint8_t flash_buffer[128];

extern FRESULT fresult;
extern FATFS fs;
extern FIL fil;

uint32_t CAN_mailbox;
HAL_StatusTypeDef CAN_status;
uint8_t CAN_Tx_buffer[8] = {0};
CAN_TxHeaderTypeDef CAN_TxHeader;
uint32_t uart_speed;

t_eeprom_settings eeprom_settings;

#define CAN_BUFFER_SIZE	60
struct
{
	uint16_t Pointer_Read;
	uint16_t Pointer_Write;
	can_msg_t Item[CAN_BUFFER_SIZE];
} CAN_Buffer, CAN_Log_Buffer;

void CAN_Buffer_Init(void)
{
	CAN_Buffer.Pointer_Read = 0;
	CAN_Buffer.Pointer_Write = 0;

	CAN_Log_Buffer.Pointer_Read = 0;
	CAN_Log_Buffer.Pointer_Write = 0;
}



HAL_StatusTypeDef CAN_Buffer_pull(void)
{
	if(CAN_Buffer.Pointer_Read != CAN_Buffer.Pointer_Write)
	{
		uint16_t length = BuildFrameToUSB(CAN_Buffer.Item[CAN_Buffer.Pointer_Read], 0, uart_tx_bufer);
		if(HAL_UART_Transmit_DMA(huart_active, uart_tx_bufer, length) == HAL_OK)
		{
			CAN_Buffer.Pointer_Read = (CAN_Buffer.Pointer_Read < (CAN_BUFFER_SIZE-1)) ? (CAN_Buffer.Pointer_Read + 1) : 0;
			return HAL_OK;
		}
		else
		{
			return HAL_ERROR;
		}
	}
	else
	{
		return HAL_BUSY;
	}
}

HAL_StatusTypeDef CAN_Log_Buffer_pull(void)
{
	if(CAN_Log_Buffer.Pointer_Read != CAN_Log_Buffer.Pointer_Write)
	{
		uint16_t length = BuildFrameToFile(CAN_Log_Buffer.Item[CAN_Log_Buffer.Pointer_Read], 0, flash_buffer);
		if(Save_to_File(flash_buffer, length) == HAL_OK)
		{
			CAN_Log_Buffer.Pointer_Read = (CAN_Log_Buffer.Pointer_Read < (CAN_BUFFER_SIZE-1)) ? (CAN_Log_Buffer.Pointer_Read + 1) : 0;
			return HAL_OK;
		}
		else
		{
			return HAL_ERROR;
		}
	}
	else
	{
		return HAL_BUSY;
	}
}

void Generate_Next_FileName(uint8_t * name)
{
	static uint32_t num_file = 0;
	uint32_t pie;
	uint8_t piece;
	uint32_t pow10 = 10000000;
	num_file++;
	if(num_file > 99999999) num_file = 0;
	pie = num_file;
	for(int i = 0; i < 7; i++)
	{
		piece = pie / pow10;
		pie -= piece * pow10;
		name[i] = piece + '0';
		pow10 /= 10;
	}
	name[7] = pie +'0';
	if(eeprom_settings.fileOutputType == BINARYFILE)
	{
		name[9] = 'L';
		name[10] = 'O';
		name[11] = 'G';
	}
	else if(eeprom_settings.fileOutputType == GVRET_FILE)
	{
		name[9] = 'C';
		name[10] = 'S';
		name[11] = 'V';
	}
	else if(eeprom_settings.fileOutputType == CRTD_FILE)
	{
		name[9] = 'C';
		name[10] = 'R';
		name[11] = 'T';
	}
	else
	{
		name[9] = 'T';
		name[10] = 'X';
		name[11] = 'T';
	}
	name[12] = 0;
}

void CAN_Buffer_clean(void)
{
	CAN_Buffer.Pointer_Read = CAN_Buffer.Pointer_Write;
}

HAL_StatusTypeDef CAN_Buffer_Write_Data(can_msg_t msg)
{
	uint16_t capacity;
	HAL_StatusTypeDef res;


    if(CAN_Buffer.Pointer_Write >= CAN_Buffer.Pointer_Read)
    {
        capacity = CAN_BUFFER_SIZE - (CAN_Buffer.Pointer_Write - CAN_Buffer.Pointer_Read);
    }
    else
    {
        capacity = CAN_Buffer.Pointer_Read - CAN_Buffer.Pointer_Write;
    }

	if(capacity > 1)
	{
		CAN_Buffer.Item[CAN_Buffer.Pointer_Write] = msg;
		CAN_Buffer.Pointer_Write = (CAN_Buffer.Pointer_Write < (CAN_BUFFER_SIZE-1)) ? (CAN_Buffer.Pointer_Write + 1) : 0;
		res = HAL_OK;
	}
	else
	{
		res = HAL_BUSY;
	}
	return res;
}

HAL_StatusTypeDef CAN_Log_Buffer_Write_Data(can_msg_t msg)
{
	uint16_t capacity;
	HAL_StatusTypeDef res;


    if(CAN_Log_Buffer.Pointer_Write >= CAN_Log_Buffer.Pointer_Read)
    {
        capacity = CAN_BUFFER_SIZE - (CAN_Log_Buffer.Pointer_Write - CAN_Log_Buffer.Pointer_Read);
    }
    else
    {
        capacity = CAN_Log_Buffer.Pointer_Read - CAN_Log_Buffer.Pointer_Write;
    }

	if(capacity > 1)
	{
		CAN_Log_Buffer.Item[CAN_Log_Buffer.Pointer_Write] = msg;
		CAN_Log_Buffer.Pointer_Write = (CAN_Log_Buffer.Pointer_Write < (CAN_BUFFER_SIZE-1)) ? (CAN_Log_Buffer.Pointer_Write + 1) : 0;
		res = HAL_OK;
	}
	else
	{
		res = HAL_BUSY;
	}
	return res;
}

uint8_t HexTo4bits (uint8_t H)
{
	if (H >= 'a')
	    H -= 0x27;		// convert chars a-f
	else if (H >= 'A')
	    H -= 0x07;		// convert chars A-F
	H -= '0';		// convert chars 0-9
	return H;
}

uint8_t HexToShort (uint8_t H, uint8_t L)
{
	if (H >= 'a')
	    H -= 0x27;		// convert chars a-f
	else if (H >= 'A')
	    H -= 0x07;		// convert chars A-F
	H -= '0';		// convert chars 0-9

	if (L > 0x60)
	    L -= 0x27;		// convert chars a-f
	else if (L > 0x40)
	    L -= 0x07;		// convert chars A-F
	L -= 0x30;		// convert chars 0-9
	return H<<4 | L;
}

//Get the value of XOR'ing all the bytes together. This creates a reasonable checksum that can be used
//to make sure nothing too stupid has happened on the comm.
uint8_t checksumCalc(uint8_t *buffer, int length)
{
    uint8_t valu = 0;
    for (int c = 0; c < length; c++) {
        valu ^= buffer[c];
    }
    return valu;
}

HAL_StatusTypeDef CAN_Init_Custom(uint32_t speed, uint32_t mode)
{
	switch(speed)
	{
		case 1000000:
			hcan.Init.Prescaler = 4;
			hcan.Init.TimeSeg1 = CAN_BS1_7TQ;
			hcan.Init.TimeSeg2 = CAN_BS2_1TQ;
			break;
		case 800000:
			hcan.Init.Prescaler = 5;
			hcan.Init.TimeSeg1 = CAN_BS1_7TQ;
			hcan.Init.TimeSeg2 = CAN_BS2_1TQ;
			break;
		case 500000:
			hcan.Init.Prescaler = 4;
			hcan.Init.TimeSeg1 = CAN_BS1_15TQ;
			hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
			break;
		case 250000:
			hcan.Init.Prescaler = 9;
			hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
			hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
			break;
		case 125000:
			hcan.Init.Prescaler = 18;
			hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
			hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
			break;
		case 100000:
			hcan.Init.Prescaler = 45;
			hcan.Init.TimeSeg1 = CAN_BS1_6TQ;
			hcan.Init.TimeSeg2 = CAN_BS2_1TQ;
			break;
		case 50000://
			hcan.Init.Prescaler = 45;
			hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
			hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
			break;
		case 20000:
			hcan.Init.Prescaler = 120;
			hcan.Init.TimeSeg1 = CAN_BS1_12TQ;
			hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
			break;
		case 10000:
			hcan.Init.Prescaler = 255;
			hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
			hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
			break;
		case 95238:
			hcan.Init.Prescaler = 27;
			hcan.Init.TimeSeg1 = CAN_BS1_11TQ;
			hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
			break;
		case 47619:
			hcan.Init.Prescaler = 54;
			hcan.Init.TimeSeg1 = CAN_BS1_11TQ;
			hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
			break;
		case 5000:
			hcan.Init.Prescaler = 450;
			hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
			hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
			break;
		case 8333:
			hcan.Init.Prescaler = 270;
			hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
			hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
			break;
		case 33333:
			hcan.Init.Prescaler = 72;
			hcan.Init.TimeSeg1 = CAN_BS1_12TQ;
			hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
			break;
		default:
			// custom btr calculation
			STM_bxCAN_calc(36000000UL, speed, &hcan);
			break;
	}

  hcan.Instance = CAN1;
  //hcan.Init.Prescaler = 16;
  hcan.Init.Mode = mode; //CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  //hcan.Init.TimeSeg1 = CAN_BS1_1TQ;
  //hcan.Init.TimeSeg2 = CAN_BS2_1TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = DISABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = DISABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;

  return HAL_CAN_Init(&hcan);
}

HAL_StatusTypeDef LIN_Init_Custom(uint32_t speed, uint32_t mode)
{
	  huart3.Instance = USART3;
	  huart3.Init.BaudRate = speed;
	  huart3.Init.WordLength = UART_WORDLENGTH_8B;
	  huart3.Init.StopBits = UART_STOPBITS_1;
	  huart3.Init.Parity = UART_PARITY_NONE;
	  huart3.Init.Mode = mode;
	  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	  if (HAL_LIN_Init(&huart1, UART_LINBREAKDETECTLENGTH_10B) != HAL_OK)
	  {
	    return HAL_ERROR;
	  }
	  return HAL_OK;
}

void ShortToHex (uint8_t in, uint8_t * out)
{
	uint8_t temp;
	temp = (in >> 4) & 0x0F;
	out[0] = (temp < 10) ? temp + '0' : temp + ('A' - 10);
	temp = in & 0x0F;
	out[1] = (temp < 10) ? temp + '0' : temp + ('A' - 10);
}

uint16_t BuildFrameToUSB (can_msg_t frame, int whichBus, uint8_t * buf)
{
	uint32_t pointer = 0;
    uint8_t temp;

    if (conf.state == LAWICEL_CONNECT)
    {
    	if(frame.header.IDE == CAN_ID_STD)//(frame.format == STANDARD)
    	{
    		if(frame.header.RTR == CAN_RTR_DATA)//(frame.type == DATA)
    		{
    			buf[pointer++] = 't';
    		}
    		else
    		{
    			buf[pointer++] = 'r';
    		}
    		buf[pointer++] = ((frame.header.StdId >> 8) < 10) ? (frame.header.StdId >> 8) + '0' : (frame.header.StdId >> 8) + ('A' - 10);
    		ShortToHex(frame.header.StdId & 0xFF, &buf[pointer]);
    		pointer +=2;
    	}
    	else //if(msg.format == EXTENDED)
    	{
    		if(frame.header.RTR == CAN_RTR_DATA)//(frame.type == DATA)
    		{
    			buf[pointer++] = 'T';
    		}
    		else
    		{
    			buf[pointer++] = 'R';
    		}
    		ShortToHex((frame.header.ExtId >> 24) & 0xFF, &buf[pointer]);
    		pointer +=2;
    		ShortToHex((frame.header.ExtId >> 16) & 0xFF, &buf[pointer]);
    		pointer +=2;
    		ShortToHex((frame.header.ExtId >> 8) & 0xFF, &buf[pointer]);
    		pointer +=2;
    		ShortToHex((frame.header.ExtId >> 0) & 0xFF, &buf[pointer]);
    		pointer +=2;
    	}

    	buf[pointer++] = (frame.header.DLC  < 10) ? frame.header.DLC + '0' : frame.header.DLC + ('A' - 10);

    	if(frame.header.RTR == CAN_RTR_DATA)//(frame.type == DATA)
    	{
    		for(int i = 0; i < frame.header.DLC; i++)
    		{
    			ShortToHex(frame.data_byte[i], &buf[pointer]);
    			pointer +=2;
    		}
    	}

    	if(conf.timestamp_en)
    	{
    		uint16_t timestamp = frame.timestamp % 60000u;
    		ShortToHex(timestamp >> 8 , &buf[pointer]);
    		pointer +=2;
    		ShortToHex(timestamp & 0xFF , &buf[pointer]);
    		pointer +=2;
    	}

    	buf[pointer++] = CR;
    }
    else if(conf.state == SAVVYCAN_CONNECT)
    {
        if (conf.useBinarySerialComm)
        {
        	uint32_t id_temp;
            if (frame.header.IDE == CAN_ID_EXT) id_temp = frame.header.ExtId | 1 << 31;
            else id_temp = frame.header.StdId;
            buf[pointer++] = 0xF1;
            buf[pointer++] = eeprom_settings.numBus; //0 = canbus frame sending
            buf[pointer++] = (uint8_t)(frame.timestamp & 0xFF);
            buf[pointer++] = (uint8_t)(frame.timestamp >> 8);
            buf[pointer++] = (uint8_t)(frame.timestamp >> 16);
            buf[pointer++] = (uint8_t)(frame.timestamp >> 24);
            buf[pointer++] = (uint8_t)(id_temp & 0xFF);
            buf[pointer++] = (uint8_t)(id_temp >> 8);
            buf[pointer++] = (uint8_t)(id_temp >> 16);
            buf[pointer++] = (uint8_t)(id_temp >> 24);
            buf[pointer++] = frame.header.DLC + (uint8_t)(whichBus << 4);
            for (int c = 0; c < frame.header.DLC; c++) {
                buf[pointer++] = frame.data_byte[c];
            }
            //temp = checksumCalc(buff, 11 + frame.length);
            temp = 0;
            buf[pointer++] = temp;

        } else {
           /* SerialUSB.print(micros());
            SerialUSB.print(" - ");
            SerialUSB.print(frame.id, HEX);
            if (frame.extended) SerialUSB.print(" X ");
            else SerialUSB.print(" S ");
            SerialUSB.print(whichBus);
            SerialUSB.print(" ");
            SerialUSB.print(frame.length);
            for (int c = 0; c < frame.length; c++) {
                SerialUSB.print(" ");
                SerialUSB.print(frame.data.bytes[c], HEX);
            }
            SerialUSB.println();*/
        }
    }
    return pointer;
}


uint16_t BuildFrameToFile(can_msg_t frame, int whichBus, uint8_t * buff)
{
   // uint8_t buff[40];
    uint8_t temp;
    uint32_t id_temp;
    uint32_t pointer = 0;
    if (eeprom_settings.fileOutputType == BINARYFILE) {
    	if (frame.header.IDE == CAN_ID_EXT) id_temp = frame.header.ExtId | 1 << 31;
    	else id_temp = frame.header.StdId;
        buff[pointer++] = (uint8_t)(frame.timestamp & 0xFF);
        buff[pointer++] = (uint8_t)(frame.timestamp >> 8);
        buff[pointer++] = (uint8_t)(frame.timestamp >> 16);
        buff[pointer++] = (uint8_t)(frame.timestamp >> 24);
        buff[pointer++] = (uint8_t)(id_temp & 0xFF);
        buff[pointer++] = (uint8_t)(id_temp >> 8);
        buff[pointer++] = (uint8_t)(id_temp >> 16);
        buff[pointer++] = (uint8_t)(id_temp >> 24);
        buff[pointer++] = frame.header.DLC + (uint8_t)(whichBus << 4);
        for (int c = 0; c < frame.header.DLC; c++) {
            buff[pointer++] = frame.data_byte[c];
        }
        //Logger::fileRaw(buff, 9 + frame.length);
    } else if (eeprom_settings.fileOutputType == GVRET_FILE) {
    	if (frame.header.IDE == CAN_ID_EXT) id_temp = frame.header.ExtId;
    	else id_temp = frame.header.StdId;
        sprintf((char *)buff, "%i,%x,%i,%i,%i", (int)frame.timestamp, id_temp, frame.header.IDE, whichBus, frame.header.DLC);
        //Logger::fileRaw(buff, strlen((char *)buff));
        pointer = strlen((char *)buff);

        if(frame.header.RTR == CAN_RTR_DATA)
    	{
    		for(int i = 0; i < frame.header.DLC; i++)
    		{
    			buff[pointer++] = ',';
    			ShortToHex(frame.data_byte[i], &buff[pointer]);
    			pointer +=2;
    		}
    	}

//        for (int c = 0; c < frame.header.DLC; c++) {
//            sprintf((char *) buff, ",%x", frame.data_byte[c]);
//            //Logger::fileRaw(buff, strlen((char *)buff));
//            //TODO
//        }
        buff[pointer++] = '\r';
        buff[pointer++] = '\n';
        //Logger::fileRaw(buff, 2);

    } else if (eeprom_settings.fileOutputType == CRTD_FILE) {
        int idBits = 11;
        if (frame.header.IDE) idBits = 29;
        if (frame.header.IDE == CAN_ID_EXT) id_temp = frame.header.ExtId;
        else id_temp = frame.header.StdId;
        sprintf((char *)buff, "%i.%u %c%i %x", frame.timestamp / 1000, frame.timestamp % 1000, (frame.can_dir == DIR_TRANSMIT)?'T':'R', idBits, id_temp);

        pointer = strlen((char *)buff);

        if(frame.header.RTR == CAN_RTR_DATA)
    	{
    		for(int i = 0; i < frame.header.DLC; i++)
    		{
    			buff[pointer++] = ' ';
    			ShortToHex(frame.data_byte[i], &buff[pointer]);
    			pointer +=2;
    		}
    	}
        //Logger::fileRaw(buff, strlen((char *)buff));

//        for (int c = 0; c < frame.header.DLC; c++) {
//            sprintf((char *) buff, " %x", frame.data_byte[c]);
//            //Logger::fileRaw(buff, strlen((char *)buff));
//        }
        buff[pointer++] = '\r';
        buff[pointer++] = '\n';
        //Logger::fileRaw(buff, 2);
    }
    return pointer;
}


void setPromiscuousMode()
{
    //By default there are 7 mailboxes for each device that are RX boxes
    //This sets each mailbox to have an open filter that will accept extended
    //or standard frames
/*    int filter;
    //extended
    for (filter = 0; filter < 3; filter++) {
        Can0.setRXFilter(filter, 0, 0, true);
        Can1.setRXFilter(filter, 0, 0, true);
    }
    //standard
    for (filter = 3; filter < 7; filter++) {
        Can0.setRXFilter(filter, 0, 0, false);
        Can1.setRXFilter(filter, 0, 0, false);
    }*/
}

HAL_StatusTypeDef SetFilterCAN(uint32_t id, uint32_t mask_or_id, uint32_t mode, uint32_t num)
{
	 if(mask_or_id == 0 && mode == CAN_FILTERMODE_IDMASK)
	 {
 		  CAN_FilterTypeDef CAN_FilterStructure;
 		  CAN_FilterStructure.FilterBank = 0;
 		  CAN_FilterStructure.FilterMode = CAN_FILTERMODE_IDMASK;
 		  CAN_FilterStructure.FilterFIFOAssignment = CAN_FILTER_FIFO1;
 		  CAN_FilterStructure.FilterMaskIdHigh = 0xFFFF;
 		  CAN_FilterStructure.FilterMaskIdLow = 0xFFFF;
 		  CAN_FilterStructure.FilterIdHigh = 0;
 		  CAN_FilterStructure.FilterIdLow = 0;
 		  CAN_FilterStructure.FilterScale = CAN_FILTERSCALE_32BIT;
 		  CAN_FilterStructure.FilterActivation = CAN_FILTER_ENABLE;
 		  //if(HAL_CAN_ConfigFilter(&hcan, &CAN_FilterStructure) != HAL_OK) return HAL_ERROR;

 		  CAN_FilterStructure.FilterIdHigh = 0;
 		  CAN_FilterStructure.FilterIdLow = 0;
 		  CAN_FilterStructure.FilterBank = 0;
 		  CAN_FilterStructure.FilterMaskIdHigh = 0;
 		  CAN_FilterStructure.FilterMaskIdLow = 0;
 		  CAN_FilterStructure.FilterFIFOAssignment = CAN_FILTER_FIFO0;
 		  if(HAL_CAN_ConfigFilter(&hcan, &CAN_FilterStructure) != HAL_OK) return HAL_ERROR;

// 		  CAN_FilterStructure.FilterActivation = CAN_FILTER_DISABLE;
// 		  for(int i = 1; i < 14; i++)
// 		  {
// 			 CAN_FilterStructure.FilterBank = i;
// 			 HAL_CAN_ConfigFilter(&hcan, &CAN_FilterStructure);
// 		  }
	 }
	 else
	 {
		  CAN_FilterTypeDef CAN_FilterStructure;
		  CAN_FilterStructure.FilterBank = num;
		  CAN_FilterStructure.FilterMode = mode;

		  CAN_FilterStructure.FilterMaskIdHigh = mask_or_id >> 16;
		  CAN_FilterStructure.FilterMaskIdLow = mask_or_id & 0xFFFF;
		  CAN_FilterStructure.FilterIdHigh = id >> 16;
		  CAN_FilterStructure.FilterIdLow = id & 0xFFFF;
		  CAN_FilterStructure.FilterScale = CAN_FILTERSCALE_32BIT;
		  CAN_FilterStructure.FilterActivation = CAN_FILTER_ENABLE;

		  CAN_FilterStructure.FilterFIFOAssignment = CAN_FILTER_FIFO0;
		  if(HAL_CAN_ConfigFilter(&hcan, &CAN_FilterStructure) != HAL_OK) return HAL_ERROR;
	 }
	 return HAL_OK;
}

uint8_t exec_usb_cmd (uint8_t * cmd_buf)
{
    uint8_t cmd_len = strlen ((char *)cmd_buf);	// get command length

    if(conf.scpipt_saving == true && cmd_buf[0] != PROGRAM_SCRIPT)
    {
    	if(conf.script_address < eeprom_settings.start_address_csript || conf.script_address > eeprom_settings.eeprom_size-2)
    	{
    		conf.script_address = eeprom_settings.start_address_csript;
    	}
    	if(conf.script_address + cmd_len >= eeprom_settings.eeprom_size-2) ;//return ERROR;
    	cmd_buf[cmd_len++] = CR;
    	if(EEPROM_Write(&hspi2, conf.script_address, cmd_buf, cmd_len) == HAL_OK)
    	{
    		conf.script_address += cmd_len;
    		//return CR;
    	}
    	//else return ERROR;
    }

    switch (cmd_buf[0]) {
            // get serial number
        case GET_SERIAL:
        	uart_tx_com_bufer[uart_tx_pointer++] = GET_SERIAL;
            uart_tx_com_bufer[uart_tx_pointer++] = '0';
            uart_tx_com_bufer[uart_tx_pointer++] = '0';
            uart_tx_com_bufer[uart_tx_pointer++] = '0';
            uart_tx_com_bufer[uart_tx_pointer++] = '1';
            return CR;

            // get hard- and software version
        case GET_VERSION:
        	uart_tx_com_bufer[uart_tx_pointer++] = (GET_VERSION);
            ShortToHex(HW_VER, &uart_tx_com_bufer[uart_tx_pointer]);
            uart_tx_pointer +=2;
            ShortToHex(SW_VER, &uart_tx_com_bufer[uart_tx_pointer]);
            uart_tx_pointer +=2;
            return CR;

            // get only software version
        case GET_SW_VERSION:
        	uart_tx_com_bufer[uart_tx_pointer++] = (GET_SW_VERSION);
            ShortToHex(SW_VER_MAJOR, &uart_tx_com_bufer[uart_tx_pointer]);
            uart_tx_pointer +=2;
            ShortToHex(SW_VER_MINOR, &uart_tx_com_bufer[uart_tx_pointer]);
            uart_tx_pointer +=2;
            return CR;

            // toggle time stamp option
        case TIME_STAMP:
            if(cmd_buf[1] == '1')
            {
            	conf.timestamp_en = true;
            }
            else if(cmd_buf[1] == '0')
            {
            	conf.timestamp_en = false;
            }
            else
            {
            	conf.timestamp_en = (conf.timestamp_en) ? false : true;
            }
            return CR;

            // read status flag
        case READ_STATUS:

        	uart_tx_com_bufer[uart_tx_pointer++] = (READ_STATUS);

            uart_tx_com_bufer[uart_tx_pointer++] = '0';
            uart_tx_com_bufer[uart_tx_pointer++] = HAL_CAN_GetState(&hcan) + '0';

            HAL_CAN_ResetError(&hcan);
            return CR;


        case SET_FILTER_ID:
        	if(cmd_len != 9) return ERROR;
            conf.filter_id = HexTo4bits(cmd_buf[1]) << 28 |
            				 HexTo4bits(cmd_buf[2]) << 24 |
							 HexTo4bits(cmd_buf[3]) << 20 |
							 HexTo4bits(cmd_buf[4]) << 16 |
							 HexTo4bits(cmd_buf[5]) << 12 |
							 HexTo4bits(cmd_buf[6]) << 8 |
							 HexTo4bits(cmd_buf[7]) << 4 |
							 HexTo4bits(cmd_buf[8]) << 0 ;
        	return CR;
        case SET_FILTER_MASK:
        	if(cmd_len != 9) return ERROR;
            conf.filter_mask = HexTo4bits(cmd_buf[1]) << 28 |
            				 HexTo4bits(cmd_buf[2]) << 24 |
							 HexTo4bits(cmd_buf[3]) << 20 |
							 HexTo4bits(cmd_buf[4]) << 16 |
							 HexTo4bits(cmd_buf[5]) << 12 |
							 HexTo4bits(cmd_buf[6]) << 8 |
							 HexTo4bits(cmd_buf[7]) << 4 |
							 HexTo4bits(cmd_buf[8]) << 0 ;
            return CR;
        case SET_FILTER:
        	if(cmd_len == 3)
        	{
        		conf.filter_num = HexTo4bits(cmd_buf[1]);
        		conf.filter_mode = cmd_buf[2] == 'I' ? CAN_FILTERMODE_IDLIST : cmd_buf[2] == 'M' ? CAN_FILTERMODE_IDMASK : 0xFF;
        	}
        	else if(cmd_len == 4)
        	{
        		conf.filter_num = HexTo4bits(cmd_buf[1]) * 10 + HexTo4bits(cmd_buf[2]);
        		conf.filter_mode = cmd_buf[3] == 'I' ? CAN_FILTERMODE_IDLIST : cmd_buf[3] == 'M' ? CAN_FILTERMODE_IDMASK : 0xFF;
        	}
        	else
        	{
        		return ERROR;
        	}
        	if(conf.filter_num > 14 || conf.filter_mode == 0xFF) return ERROR;

        	Close_CAN_cannel();
        	SetFilterCAN(conf.filter_id, conf.filter_mask, conf.filter_mode, conf.filter_num);
        	Open_CAN_cannel();
        	return CR;

            // set bitrate via BTR
        case SET_CUSTOM_BTR:
        	eeprom_settings.CAN_Speed[eeprom_settings.numBus] = 0;
        	for(int i = 1; i < cmd_len; i++)
        	{
        		if(cmd_buf[i] < '0' || cmd_buf[i] > '9') return ERROR;
        		eeprom_settings.CAN_Speed[eeprom_settings.numBus] *= 10;
        		eeprom_settings.CAN_Speed[eeprom_settings.numBus] += HexTo4bits(cmd_buf[i]);
        	}

        	return CR; // set custom speed
            // set fix bitrate
        case SET_BITRATE:

            if ((cmd_len != 5) && (cmd_len != 2))
                return ERROR;	// check valid cmd length

            // check if CAN controller is in reset mode
            if (conf.state != IDLE_ST)
                return ERROR;

            switch(cmd_buf[1])
            {
            case '0':
            	eeprom_settings.CAN_Speed[eeprom_settings.numBus] = 10000;
            	break;
            case '1':
            	eeprom_settings.CAN_Speed[eeprom_settings.numBus] = 20000;
            	break;
            case '2':
            	eeprom_settings.CAN_Speed[eeprom_settings.numBus] = 50000;
            	break;
            case '3':
            	eeprom_settings.CAN_Speed[eeprom_settings.numBus] = 100000;
            	break;
            case '4':
            	eeprom_settings.CAN_Speed[eeprom_settings.numBus] = 125000;
            	break;
            case '5':
            	eeprom_settings.CAN_Speed[eeprom_settings.numBus] = 250000;
            	break;
            case '6':
            	eeprom_settings.CAN_Speed[eeprom_settings.numBus] = 500000;
            	break;
            case '7':
            	eeprom_settings.CAN_Speed[eeprom_settings.numBus] = 800000;
            	break;
            case '8':
            	eeprom_settings.CAN_Speed[eeprom_settings.numBus] = 1000000;
            	break;
            case '9':
            	eeprom_settings.CAN_Speed[eeprom_settings.numBus] = 95238;
            	break;
            case 'a':
            case 'A':
            	eeprom_settings.CAN_Speed[eeprom_settings.numBus] = 8333;
            	break;
            case 'b':
            case 'B':
            	eeprom_settings.CAN_Speed[eeprom_settings.numBus] = 47619;
            	break;
            case 'c':
            case 'C':
            	eeprom_settings.CAN_Speed[eeprom_settings.numBus] = 33333;
            	break;
            case 'd':
            case 'D':
            	eeprom_settings.CAN_Speed[eeprom_settings.numBus] = 5000;
            	break;
            default:
            	return ERROR;
            	break;
            }
            return CR;


            // open CAN channel
        case OPEN_CAN_CHAN:

            // return error if controller is not initialized or already open
        	if(eeprom_settings.numBus == 3) // LIN
        	{
            	eeprom_settings.CAN_mode[eeprom_settings.numBus] = CAN_MODE_NORMAL;
            	if(Open_LIN_cannel() != HAL_OK) return ERROR;
        	}
        	else // CAN
        	{
                if (HAL_CAN_GetState(&hcan) != HAL_CAN_STATE_RESET) return ERROR;

            	eeprom_settings.CAN_mode[eeprom_settings.numBus] = CAN_MODE_NORMAL;
            	if(Open_CAN_cannel() != HAL_OK) return ERROR;
        	}
        	conf.CAN_Enable[eeprom_settings.numBus] = true;

    		conf.state = LAWICEL_CONNECT;
            return CR;

            // close CAN channel

        case CLOSE_CAN_CHAN:
        	conf.state = IDLE_ST;
        	if(eeprom_settings.numBus == 3) // LIN
        	{
            	if(Close_LIN_cannel() != HAL_OK) return ERROR;
        	}
        	else // CAN
        	{
        		HAL_CAN_ResetError(&hcan);
        		if(Close_CAN_cannel() != HAL_OK) return ERROR;
        	}
        	conf.CAN_Enable[eeprom_settings.numBus] = false;
            return CR;

            // send 11bit ID message

        case SEND_R11BIT_ID:

            // check if CAN controller is in reset mode or busy
            if (conf.state == IDLE_ST)
            {
            	return ERROR;
            }

            // check valid cmd length (only 5 bytes for RTR)
            if (cmd_len != 5)
            {
            	return ERROR;
            }

        	CAN_TxHeader.StdId = (HexTo4bits(cmd_buf[1]) << 8) | HexToShort(cmd_buf[2], cmd_buf[3]);

        	CAN_TxHeader.IDE = CAN_ID_STD;
        	CAN_TxHeader.RTR = CAN_RTR_REMOTE;
        	CAN_TxHeader.DLC = HexTo4bits(cmd_buf[4]);
        	HAL_GPIO_WritePin(TX_LED_GPIO_Port, TX_LED_Pin, GPIO_PIN_SET);
        	CAN_status = HAL_CAN_AddTxMessage(&hcan, &CAN_TxHeader, CAN_Tx_buffer, &CAN_mailbox);
        	if(CAN_status != HAL_OK)
        	{
        		return ERROR;
        	}

        	return CR;


        case SEND_11BIT_ID:

            // check if CAN controller is in reset mode or busy
            if (conf.state == IDLE_ST)
            {
            	return ERROR;
            }

            if ((cmd_len < 5) || (cmd_len > 21))
            {
            	return ERROR;	// check valid cmd length
            }

        	CAN_TxHeader.StdId = (HexTo4bits(cmd_buf[1]) << 8) | HexToShort(cmd_buf[2], cmd_buf[3]);

        	CAN_TxHeader.IDE = CAN_ID_STD;
        	CAN_TxHeader.RTR = CAN_RTR_DATA;
        	CAN_TxHeader.DLC = HexTo4bits(cmd_buf[4]);

        	// check number of data bytes supplied against data lenght byte
            if (CAN_TxHeader.DLC != ((cmd_len - 5) / 2))
            {
            	return ERROR;
            }

            // check for valid length
            if (CAN_TxHeader.DLC > 8)
            {
            	return ERROR;
            }

        	for(int i = 0; i < CAN_TxHeader.DLC; i++)
        	{
        		CAN_Tx_buffer[i] = HexToShort(cmd_buf[i*2+5], cmd_buf[i*2+6]);
        	}
        	HAL_GPIO_WritePin(TX_LED_GPIO_Port, TX_LED_Pin, GPIO_PIN_SET);
        	CAN_status = HAL_CAN_AddTxMessage(&hcan, &CAN_TxHeader, CAN_Tx_buffer, &CAN_mailbox);
        	if(CAN_status != HAL_OK)
        	{
        		return ERROR;
        	}

        	return CR;


            // send 29bit ID message

        case SEND_R29BIT_ID:

            if (conf.state == IDLE_ST) return ERROR;
            if (cmd_len != 10)      return ERROR;	// check valid cmd length

        	CAN_TxHeader.ExtId = (HexToShort(cmd_buf[1],cmd_buf[2]) << 24) |
        						 (HexToShort(cmd_buf[3],cmd_buf[4]) << 16) |
								 (HexToShort(cmd_buf[5],cmd_buf[6]) << 8) |
								  HexToShort(cmd_buf[7],cmd_buf[8]);

        	CAN_TxHeader.IDE = CAN_ID_EXT;
        	CAN_TxHeader.RTR = CAN_RTR_REMOTE;
        	CAN_TxHeader.DLC = HexTo4bits(cmd_buf[9]);
        	HAL_GPIO_WritePin(TX_LED_GPIO_Port, TX_LED_Pin, GPIO_PIN_SET);
        	CAN_status = HAL_CAN_AddTxMessage(&hcan, &CAN_TxHeader, CAN_Tx_buffer, &CAN_mailbox);
        	if(CAN_status != HAL_OK)
        	{
        		return ERROR;
        	}

        	return CR;


        case SEND_29BIT_ID:

            // check if CAN controller is in reset mode or busy
            if (conf.state == IDLE_ST) return ERROR;

            if ((cmd_len < 10) || (cmd_len > 26))
            {
            	return ERROR;	// check valid cmd length
            }

        	CAN_TxHeader.ExtId = (HexToShort(cmd_buf[1],cmd_buf[2]) << 24) |
        						 (HexToShort(cmd_buf[3],cmd_buf[4]) << 16) |
								 (HexToShort(cmd_buf[5],cmd_buf[6]) << 8) |
								 HexToShort(cmd_buf[7], cmd_buf[8]);

        	CAN_TxHeader.IDE = CAN_ID_EXT;
        	CAN_TxHeader.RTR = CAN_RTR_DATA;
        	CAN_TxHeader.DLC = HexTo4bits(cmd_buf[9]);

        	// check number of data bytes supplied against data lenght byte
            if (CAN_TxHeader.DLC != ((cmd_len - 10) / 2))
            {
            	return ERROR;
            }

            // check for valid length
            if (CAN_TxHeader.DLC > 8)
            {
            	return ERROR;
            }

        	for(int i = 0; i < CAN_TxHeader.DLC; i++)
        	{
        		CAN_Tx_buffer[i] = HexToShort(cmd_buf[i*2+10], cmd_buf[i*2+11]);
        	}
        	HAL_GPIO_WritePin(TX_LED_GPIO_Port, TX_LED_Pin, GPIO_PIN_SET);
        	CAN_status = HAL_CAN_AddTxMessage(&hcan, &CAN_TxHeader, CAN_Tx_buffer, &CAN_mailbox);
        	if(CAN_status != HAL_OK)
        		return ERROR;

        	return CR;

            // read Error Capture Register
            // read Arbitration Lost Register
        case READ_ECR:
        case READ_ALCR:
            return CR;

            // read SJA1000 register
        case READ_REG_COM:
            return CR;

            // write SJA1000 register
        case WRITE_REG_COM:
            return CR;

        case LISTEN_ONLY:
        	eeprom_settings.CAN_mode[eeprom_settings.numBus] = CAN_MODE_SILENT;
        	if(Open_CAN_cannel() != HAL_OK) return ERROR;
    		conf.state = LAWICEL_CONNECT;
    		conf.CAN_Enable[eeprom_settings.numBus] = true;
            return CR;

            //Set USART bitrate
        case SET_USART_BTR:
        	switch (cmd_buf[1])
        	{
        	case '0': eeprom_settings.UART_Speed = 230400; break;
        	case '1': eeprom_settings.UART_Speed = 115200; break;
        	case '2': eeprom_settings.UART_Speed = 57600; break;
        	case '3': eeprom_settings.UART_Speed = 38400; break;
        	case '4': eeprom_settings.UART_Speed = 19200; break;
        	case '5': eeprom_settings.UART_Speed = 9600; break;
        	case '6': eeprom_settings.UART_Speed = 2400; break;
        	default: eeprom_settings.UART_Speed = 1000000; break;
        	}
        	HAL_UART_DeInit(huart_active);
        	huart3.Init.BaudRate = eeprom_settings.UART_Speed;
        	if (HAL_UART_Init(huart_active) != HAL_OK)
        	{
        		return ERROR;
        	}
        	EEPROM_Write(&hspi2, EEPROM_SETINGS_ADDR + ((uint32_t)&eeprom_settings.UART_Speed - (uint32_t)&eeprom_settings), (uint8_t*)&eeprom_settings.UART_Speed, sizeof(eeprom_settings.UART_Speed));
        	return CR; // set predefined speed

        case SET_USART_BTR_CUST:
        	eeprom_settings.UART_Speed = 0;
        	for(int i = 1; i < cmd_len; i++)
        	{
        		if(cmd_buf[i] < '0' || cmd_buf[i] > '9') return ERROR;
        		eeprom_settings.UART_Speed *= 10;
        		eeprom_settings.UART_Speed += HexTo4bits(cmd_buf[i]);
        	}
        	if(eeprom_settings.UART_Speed < 200 || eeprom_settings.UART_Speed > 2000000) return ERROR;

        	HAL_UART_DeInit(huart_active);
        	huart3.Init.BaudRate = eeprom_settings.UART_Speed;
        	if (HAL_UART_Init(huart_active) != HAL_OK)
        	{
        		return ERROR;
        	}
        	EEPROM_Write(&hspi2, EEPROM_SETINGS_ADDR + ((uint32_t)&eeprom_settings.UART_Speed - (uint32_t)&eeprom_settings), (uint8_t*)&eeprom_settings.UART_Speed, sizeof(eeprom_settings.UART_Speed));
        	return CR; // set custom speed

        	// Select Channel
        case SELECT_BUS_CHANEL:
        	if(cmd_buf[1] < '0' || cmd_buf[1] > '4') return ERROR;
        	eeprom_settings.numBus = HexTo4bits(cmd_buf[1]) - 1;
        	//Change_CAN_channel();
        	Open_CAN_cannel();
        	return CR;


        case PROGRAM_SCRIPT:
            if(cmd_buf[1] == '1')
            {
            	conf.scpipt_saving = true;
            	conf.state = IDLE_ST;
            	conf.script_address = eeprom_settings.start_address_csript;
            }
            else if(cmd_buf[1] == '0')
            {
            	uint8_t terminator = 0xFF;
            	conf.scpipt_saving = false;
            	EEPROM_Write(&hspi2, conf.script_address, &terminator, 1);
            }
            else if(cmd_buf[1] == '2')
            {
            	conf.script_print = true;
            }
        	return CR;

        case JUMP_MARKER:
        	if(conf.script_run)	conf.script_loop_address = conf.script_address;
        	break;

        case DELAY_MS:
        	conf.script_delay = 0;
        	for(int i = 1; i < cmd_len; i++)
        	{
        		if(cmd_buf[i] < '0' || cmd_buf[i] > '9') return ERROR;
        		conf.script_delay *= 10;
        		conf.script_delay += HexTo4bits(cmd_buf[i]);
        	}
        	conf.script_timestamp = HAL_GetTick();
        	conf.script_delay_active = true;
        	break;

        case STOP_SCRIPT:
        	conf.script_run = false;
        	break;

        case START_SCRIPT:
        	conf.script_address = eeprom_settings.start_address_csript;
        	conf.script_run = true;
        	break;

        case STOP_LOGGING:

        	break;

        case START_LOGGING:

        	break;

        case HELP:
        	conf.help_print = true;
        	conf.help_text_pointer = 0;
        	break;

        case RESET:
        	if(cmd_buf[1] == 'R' && cmd_buf[2] == 'S' && cmd_buf[3] == 'T')
        	{
        		NVIC_SystemReset();
        	}
        	break;

            // end with error on unknown commands
        default:

            return ERROR;

    }				// end switch

    // we should never reach this return
    return ERROR;

}				// end exec_usb_cmd

void Check_Command(uint8_t in_byte)
{
    static int step = 0;
    static STATE state = IDLE;
    static uint32_t build_int;
    static int serialCnt = 0;
    //static uint8_t buff[20];
    uint32_t now;

    serialCnt++;
    switch (state) {
    case IDLE:
        if (in_byte == 0xF1) state = GET_COMMAND;
        else if (in_byte == 0xE7) {
            conf.useBinarySerialComm = true;
            conf.state = SAVVYCAN_CONNECT;
            //setPromiscuousMode(); //go into promisc. mode with binary comm
        }
        else {
        	if (in_byte == CR)	// check for end of command
	        {
	            // Execute command and return status to terminal
	        	uint8_t temp_char;
	            temp_char = exec_usb_cmd (cmd_buf);

	            uart_tx_com_bufer[uart_tx_pointer++] = temp_char;
	            uart_answ_ready = 1;

	            // flush command buffer
	            for (buf_ind = 0; buf_ind < CMD_BUFFER_LENGTH; buf_ind++)
	            {
	                cmd_buf[buf_ind] = 0x00;
	            }
	            buf_ind = 0;	// point to start of command
	        }
	        else if (in_byte != 0)	// store new char in buffer
	        {
	            cmd_buf[buf_ind] = in_byte;	// store char
	            // check for buffer overflow
	            if (buf_ind < sizeof (cmd_buf) - 1)
	            {
	                buf_ind++;
	            }
	        }
        }
        break;
    case GET_COMMAND:
        switch (in_byte) {
        case PROTO_BUILD_CAN_FRAME:
            state = BUILD_CAN_FRAME;
            uart_tx_com_bufer[0] = 0xF1;
            step = 0;
            break;
        case PROTO_TIME_SYNC:
            state = IDLE;//TIME_SYNC;
            step = 0;
            now = HAL_GetTick();
            uart_tx_com_bufer[uart_tx_pointer++] = 0xF1;
            uart_tx_com_bufer[uart_tx_pointer++] = 1; //time sync
            uart_tx_com_bufer[uart_tx_pointer++] = (uint8_t)(now & 0xFF);
            uart_tx_com_bufer[uart_tx_pointer++] = (uint8_t)(now >> 8);
            uart_tx_com_bufer[uart_tx_pointer++] = (uint8_t)(now >> 16);
            uart_tx_com_bufer[uart_tx_pointer++] = (uint8_t)(now >> 24);
            uart_answ_ready = 1;
            break;
        case PROTO_DIG_INPUTS:
            //immediately return the data for digital inputs
            /*temp8 = getDigital(0) + (getDigital(1) << 1) + (getDigital(2) << 2) + (getDigital(3) << 3);
            uart_tx_com_bufer[0] = 0xF1;
            uart_tx_com_bufer[1] = 2; //digital inputs
            uart_tx_com_bufer[2] = temp8;
            temp8 = checksumCalc(uart_tx_com_bufer, 2);
            uart_tx_com_bufer[3] = temp8;
            SerialUSB.write(uart_tx_com_bufer, 4);*/
            state = IDLE;
            break;
        case PROTO_ANA_INPUTS:
            //immediately return data on analog inputs
           /* temp16 = getAnalog(0);
            uart_tx_com_bufer[0] = 0xF1;
            uart_tx_com_bufer[1] = 3;
            uart_tx_com_bufer[2] = temp16 & 0xFF;
            uart_tx_com_bufer[3] = uint8_t(temp16 >> 8);
            temp16 = getAnalog(1);
            uart_tx_com_bufer[4] = temp16 & 0xFF;
            uart_tx_com_bufer[5] = uint8_t(temp16 >> 8);
            temp16 = getAnalog(2);
            uart_tx_com_bufer[6] = temp16 & 0xFF;
            uart_tx_com_bufer[7] = uint8_t(temp16 >> 8);
            temp16 = getAnalog(3);
            uart_tx_com_bufer[8] = temp16 & 0xFF;
            uart_tx_com_bufer[9] = uint8_t(temp16 >> 8);
            temp8 = checksumCalc(uart_tx_com_bufer, 9);
            uart_tx_com_bufer[10] = temp8;
            SerialUSB.write(uart_tx_com_bufer, 11);*/
            state = IDLE;
            break;
        case PROTO_SET_DIG_OUT:
            state = SET_DIG_OUTPUTS;
            uart_tx_com_bufer[0] = 0xF1;
            break;
        case PROTO_SETUP_CANBUS:
            state = SETUP_CANBUS;
            step = 0;
            uart_tx_com_bufer[0] = 0xF1;
            break;
        case PROTO_GET_CANBUS_PARAMS:
            //immediately return data on canbus params



        	uart_tx_com_bufer[uart_tx_pointer++] = 0xF1;
        	uart_tx_com_bufer[uart_tx_pointer++] = 6;
        	uart_tx_com_bufer[uart_tx_pointer++] = conf.CAN_Enable[0] + (eeprom_settings.CAN_mode[0] == CAN_MODE_SILENT ? 1 << 4 : 0);
        	uart_tx_com_bufer[uart_tx_pointer++] = eeprom_settings.CAN_Speed[0];
        	uart_tx_com_bufer[uart_tx_pointer++] = eeprom_settings.CAN_Speed[0] >> 8;
        	uart_tx_com_bufer[uart_tx_pointer++] = eeprom_settings.CAN_Speed[0] >> 16;
        	uart_tx_com_bufer[uart_tx_pointer++] = eeprom_settings.CAN_Speed[0] >> 24;
        	uart_tx_com_bufer[uart_tx_pointer++] = conf.CAN_Enable[1] + (eeprom_settings.CAN_mode[1] == CAN_MODE_SILENT ? 1 << 4 : 0);
        	uart_tx_com_bufer[uart_tx_pointer++] = eeprom_settings.CAN_Speed[1];
        	uart_tx_com_bufer[uart_tx_pointer++] = eeprom_settings.CAN_Speed[1] >> 8;
        	uart_tx_com_bufer[uart_tx_pointer++] = eeprom_settings.CAN_Speed[1] >> 16;
        	uart_tx_com_bufer[uart_tx_pointer++] = eeprom_settings.CAN_Speed[1] >> 24;
            uart_answ_ready = 1;
            state = IDLE;
            break;
        case PROTO_GET_DEV_INFO:
            //immediately return device information
        	uart_tx_com_bufer[uart_tx_pointer++] = 0xF1;
        	uart_tx_com_bufer[uart_tx_pointer++] = 7;
        	uart_tx_com_bufer[uart_tx_pointer++] = CFG_BUILD_NUM & 0xFF;
        	uart_tx_com_bufer[uart_tx_pointer++] = (CFG_BUILD_NUM >> 8);
        	uart_tx_com_bufer[uart_tx_pointer++] = EEPROM_VER;
        	uart_tx_com_bufer[uart_tx_pointer++] = 1;//(unsigned char)settings.fileOutputType;
        	uart_tx_com_bufer[uart_tx_pointer++] = 1;//(unsigned char)settings.autoStartLogging;
        	uart_tx_com_bufer[uart_tx_pointer++] = 1;//settings.singleWire_Enabled;
            uart_answ_ready = 1;
            state = IDLE;
            break;
        case PROTO_SET_SW_MODE:
            uart_tx_com_bufer[0] = 0xF1;
            state = SET_SINGLEWIRE_MODE;
            step = 0;
            break;
        case PROTO_KEEPALIVE:
        	uart_tx_com_bufer[uart_tx_pointer++] = 0xF1;
        	uart_tx_com_bufer[uart_tx_pointer++] = 0x09;
        	uart_tx_com_bufer[uart_tx_pointer++] = 0xDE;
        	uart_tx_com_bufer[uart_tx_pointer++] = 0xAD;
            uart_answ_ready = 1;
            state = IDLE;
            break;
        case PROTO_SET_SYSTYPE:
            uart_tx_com_bufer[0] = 0xF1;
            state = SET_SYSTYPE;
            step = 0;
            break;
        case PROTO_ECHO_CAN_FRAME:
            state = ECHO_CAN_FRAME;
            uart_tx_com_bufer[0] = 0xF1;
            step = 0;
            break;
        case PROTO_GET_NUMBUSES:
            uart_tx_com_bufer[uart_tx_pointer++] = 0xF1;
            uart_tx_com_bufer[uart_tx_pointer++] = 12;
            uart_tx_com_bufer[uart_tx_pointer++] = eeprom_settings.number_of_busses; //CAN0, CAN1, SWCAN
            uart_answ_ready = 1;
            state = IDLE;
            break;
         case PROTO_GET_EXT_BUSES:
            uart_tx_com_bufer[0] = 0xF1;
            uart_tx_com_bufer[1] = 13;
            uart_tx_com_bufer[2] = conf.CAN_Enable[2] + (eeprom_settings.CAN_mode[2] == CAN_MODE_SILENT ? 1 << 4 : 0);//settings.CAN2_Enabled + (eeprom_settings.CAN_mode[2] == CAN_MODE_SILENT ? 1 << 4 : 0);
            uart_tx_com_bufer[3] = eeprom_settings.CAN_Speed[2];
            uart_tx_com_bufer[4] = eeprom_settings.CAN_Speed[2] >> 8;
            uart_tx_com_bufer[5] = eeprom_settings.CAN_Speed[2] >> 16;
            uart_tx_com_bufer[6] = eeprom_settings.CAN_Speed[2] >> 24;
            uart_tx_com_bufer[7] = conf.CAN_Enable[3] + (eeprom_settings.CAN_mode[3] == CAN_MODE_SILENT ? 1 << 4 : 0);//settings.CAN3_Enabled + (eeprom_settings.CAN_mode[3] == CAN_MODE_SILENT ? 1 << 4 : 0); //fourth bus enabled
            uart_tx_com_bufer[8] = eeprom_settings.CAN_Speed[3]; //fourth bus speed (4 bytes)
            uart_tx_com_bufer[9] = eeprom_settings.CAN_Speed[3] >> 8;
            uart_tx_com_bufer[10] = eeprom_settings.CAN_Speed[3] >> 16;
            uart_tx_com_bufer[11] = eeprom_settings.CAN_Speed[3] >> 24;
            uart_tx_com_bufer[12] = 0; //fifth bus enabled
            uart_tx_com_bufer[13] = 0; //fifth bus speed (4 bytes)
            uart_tx_com_bufer[14] = 0;
            uart_tx_com_bufer[15] = 0;
            uart_tx_com_bufer[16] = 0;
            //SerialUSB.write(uart_tx_com_bufer, 17);
            state = IDLE;
            break;
         case PROTO_SET_EXT_BUSES:
            state = SETUP_EXT_BUSES;
            step = 0;
            uart_tx_com_bufer[0] = 0xF1;
            break;
        }
        break;
    case BUILD_CAN_FRAME:
        uart_tx_com_bufer[1 + step] = in_byte;
        switch (step) {
        case 0:
        	CAN_TxHeader.StdId = in_byte;
            break;
        case 1:
        	CAN_TxHeader.StdId |= in_byte << 8;
            break;
        case 2:
        	CAN_TxHeader.StdId |= in_byte << 16;
            break;
        case 3:
        	CAN_TxHeader.StdId |= in_byte << 24;
            if (CAN_TxHeader.StdId & 1 << 31) {
            	CAN_TxHeader.ExtId = CAN_TxHeader.StdId & 0x7FFFFFFF;
            	CAN_TxHeader.IDE = CAN_ID_EXT;
            } else CAN_TxHeader.IDE = CAN_ID_STD;
            break;
        case 4:
            //out_bus = in_byte & 3; // One bus
            break;
        case 5:
        	CAN_TxHeader.DLC = in_byte & 0xF;
            if (CAN_TxHeader.DLC > 8) CAN_TxHeader.DLC = 8;
            break;
        default:
            if (step < CAN_TxHeader.DLC + 6) {
            	CAN_Tx_buffer[step - 6] = in_byte;
            } else {
                state = IDLE;
                //this would be the checksum byte. Compute and compare.
                //temp8 = checksumCalc(uart_tx_com_bufer, step);
                //if (temp8 == in_byte)
                //{
                /*if (settings.singleWire_Enabled == 1) {
                    if (build_out_frame.id == 0x100) {
                        if ( ((out_bus == 1) && !SysSettings.dedicatedSWCAN) || (out_bus == 2) ) {
                            setSWCANWakeup();
                            delay(1);
                        }
                    }
                }
                build_out_frame.rtr = 0;
                if (out_bus == 0) Can0.sendFrame(build_out_frame);
                if (out_bus == 1) Can1.sendFrame(build_out_frame);
                if (out_bus == 2) SWCAN.sendFrame(build_out_frame);

                if (settings.singleWire_Enabled == 1) {
                    if (build_out_frame.id == 0x100) {
                        if ( ((out_bus == 1) && !SysSettings.dedicatedSWCAN) || (out_bus == 2) ) {
                            delay(1);
                            setSWCANEnabled();
                        }
                    }
                }*/
                //}
                HAL_GPIO_WritePin(TX_LED_GPIO_Port, TX_LED_Pin, GPIO_PIN_SET);
                CAN_TxHeader.RTR = CAN_RTR_DATA;
                HAL_CAN_AddTxMessage(&hcan, &CAN_TxHeader, CAN_Tx_buffer, &CAN_mailbox);
            }
            break;
        }
        step++;
        break;
    case TIME_SYNC:
        state = IDLE;
        break;
    case SET_DIG_OUTPUTS: //todo: validate the XOR byte
        uart_tx_com_bufer[1] = in_byte;
        //temp8 = checksumCalc(uart_tx_com_bufer, 2);
     /*   for (int c = 0; c < 8; c++) {
            if (in_byte & (1 << c)) setOutput(c, true);
            else setOutput(c, false);
        }*/
        state = IDLE;
        break;
    case SETUP_CANBUS: //todo: validate checksum
        switch (step) {
        case 0:
            build_int = in_byte;
            break;
        case 1:
            build_int |= in_byte << 8;
            break;
        case 2:
            build_int |= in_byte << 16;
            break;
        case 3:
            build_int |= in_byte << 24;
            if (build_int > 0) {
            	Close_CAN_cannel();
            	eeprom_settings.CAN_Speed[0] = build_int & 0xFFFFF;
                if (build_int & 0x80000000) { //signals that enabled and listen only status are also being passed
                	if ((build_int & 0x60000000) == 0x60000000) {
                		conf.CAN_Enable[0] = true;
                        eeprom_settings.CAN_mode[0] = CAN_MODE_SILENT;
                        conf.state = SAVVYCAN_CONNECT;
                        eeprom_settings.numBus = 0;
                        Open_CAN_cannel();
                    } else if (build_int & 0x40000000) {
                    	conf.CAN_Enable[0] = true;
                        eeprom_settings.CAN_mode[0] = CAN_MODE_NORMAL;
                        conf.state = SAVVYCAN_CONNECT;
                        eeprom_settings.numBus = 0;
                        Open_CAN_cannel();
                    } else {
                    	conf.CAN_Enable[0] = false;
                        //Close_CAN_cannel();
                    }
                } else {
                	conf.state = SAVVYCAN_CONNECT;
                	eeprom_settings.CAN_mode[0] = CAN_MODE_NORMAL;
                	eeprom_settings.numBus = 0;
                    Open_CAN_cannel(); //if not using extended status mode then just default to enabling - this was old behavior
                    conf.CAN_Enable[0] = true;
                }

            } else { //disable first canbus
                //Close_CAN_cannel();
                conf.CAN_Enable[0] = false;
            }
            break;
        case 4:
            build_int = in_byte;
            break;
        case 5:
            build_int |= in_byte << 8;
            break;
        case 6:
            build_int |= in_byte << 16;
            break;
        case 7:
            build_int |= in_byte << 24;
            if (build_int > 0 ) {

            	eeprom_settings.CAN_Speed[1] = build_int & 0xFFFFF;
            	if(conf.CAN_Enable[0] == false)
            	{
                	Close_CAN_cannel();

                    if (build_int & 0x80000000) { //signals that enabled and listen only status are also being passed
                    	if ((build_int & 0x60000000) == 0x60000000) {
                    		conf.CAN_Enable[1] = true;
                            eeprom_settings.CAN_mode[1] = CAN_MODE_SILENT;
                            conf.state = SAVVYCAN_CONNECT;
                            eeprom_settings.numBus = 1;
                            Open_CAN_cannel();
                        } else if (build_int & 0x40000000) {
                        	conf.CAN_Enable[1] = true;
                            eeprom_settings.CAN_mode[1] = CAN_MODE_NORMAL;
                            conf.state = SAVVYCAN_CONNECT;
                            eeprom_settings.numBus = 1;
                            Open_CAN_cannel();
                        } else {
                        	conf.CAN_Enable[1] = false;
                            //Close_CAN_cannel();
                        }
                    } else {
                    	conf.state = SAVVYCAN_CONNECT;
                    	eeprom_settings.CAN_mode[1] = CAN_MODE_NORMAL;
                    	eeprom_settings.numBus = 1;
                        Open_CAN_cannel(); //if not using extended status mode then just default to enabling - this was old behavior
                        conf.CAN_Enable[1] = true;
                    }
            	}


            } else { //disable first canbus
                //Close_CAN_cannel();
                conf.CAN_Enable[1] = false;
            }
            state = IDLE;
            //now, write out the new canbus settings to EEPROM
            EEPROM_Write(&hspi2, EEPROM_SETINGS_ADDR + ((uint32_t)&eeprom_settings.CAN_Speed[0] - (uint32_t)&eeprom_settings),
            		(uint8_t*)&eeprom_settings.CAN_Speed[0], sizeof(eeprom_settings.CAN_Speed[0])*2);
            setPromiscuousMode();
            break;
        }
        step++;
        break;
    case SET_SINGLEWIRE_MODE:
        if (in_byte == 0x10) {
            //settings.singleWire_Enabled = true;
            //setSWCANEnabled();
        } else {
            //settings.singleWire_Enabled = false;
            //setSWCANSleep();
        }
        //EEPROM.write(EEPROM_PAGE, settings);
        state = IDLE;
        break;
    case SET_SYSTYPE:
        //settings.sysType = in_byte;
        //EEPROM.write(EEPROM_PAGE, settings);
        //loadSettings();
        state = IDLE;
        break;
    case ECHO_CAN_FRAME:
        uart_tx_com_bufer[1 + step] = in_byte;
        /*switch (step) {
        case 0:
            build_out_frame.id = in_byte;
            break;
        case 1:
            build_out_frame.id |= in_byte << 8;
            break;
        case 2:
            build_out_frame.id |= in_byte << 16;
            break;
        case 3:
            build_out_frame.id |= in_byte << 24;
            if (build_out_frame.id & 1 << 31) {
                build_out_frame.id &= 0x7FFFFFFF;
                build_out_frame.extended = true;
            } else build_out_frame.extended = false;
            break;
        case 4:
            out_bus = in_byte & 1;
            break;
        case 5:
            build_out_frame.length = in_byte & 0xF;
            if (build_out_frame.length > 8) build_out_frame.length = 8;
            break;
        default:
            if (step < build_out_frame.length + 6) {
                build_out_frame.data.bytes[step - 6] = in_byte;
            } else {
                state = IDLE;
                //this would be the checksum byte. Compute and compare.
                temp8 = checksumCalc(uart_tx_com_bufer, step);
                //if (temp8 == in_byte)
                //{
                toggleRXLED();
                if (isConnected) sendFrameToUSB(build_out_frame, 0);
                //}
            }
            break;
        }*/
        step++;
        break;
    case SETUP_EXT_BUSES: //setup enable/listenonly/speed for SWCAN, Enable/Speed for LIN1, LIN2
        switch (step) {
        case 0:
            build_int = in_byte;
            break;
        case 1:
            build_int |= in_byte << 8;
            break;
        case 2:
            build_int |= in_byte << 16;
            break;
        case 3:
            build_int |= in_byte << 24;
            if (build_int > 0 ) {

            	//eeprom_settings.CAN_Speed[2] = build_int & 0xFFFFF;
            	if(conf.CAN_Enable[0] == false && conf.CAN_Enable[1] == false)
            	{
                	Close_CAN_cannel();

                    if (build_int & 0x80000000) { //signals that enabled and listen only status are also being passed
                    	if ((build_int & 0x60000000) == 0x60000000) {
                    		conf.CAN_Enable[2] = true;
                            eeprom_settings.CAN_mode[2] = CAN_MODE_SILENT;
                            conf.state = SAVVYCAN_CONNECT;
                            eeprom_settings.numBus = 2;
                            Open_CAN_cannel();
                        } else if (build_int & 0x40000000) {
                        	conf.CAN_Enable[2] = true;
                            eeprom_settings.CAN_mode[2] = CAN_MODE_NORMAL;
                            conf.state = SAVVYCAN_CONNECT;
                            eeprom_settings.numBus = 2;
                            Open_CAN_cannel();
                        } else {
                        	conf.CAN_Enable[2] = false;
                            //Close_CAN_cannel();
                        }
                    } else {
                    	conf.state = SAVVYCAN_CONNECT;
                    	eeprom_settings.CAN_mode[2] = CAN_MODE_NORMAL;
                    	eeprom_settings.numBus = 2;
                        Open_CAN_cannel(); //if not using extended status mode then just default to enabling - this was old behavior
                        conf.CAN_Enable[2] = true;
                    }
            	}


            } else { //disable first canbus
                //Close_CAN_cannel();
                conf.CAN_Enable[2] = false;
            }
            break;
        case 4:
            build_int = in_byte;
            break;
        case 5:
            build_int |= in_byte << 8;
            break;
        case 6:
            build_int |= in_byte << 16;
            break;
        case 7:
            build_int |= in_byte << 24;
            if (build_int > 0 ) {

            	//eeprom_settings.CAN_Speed[3] = build_int & 0xFFFFF;
            	if(conf.CAN_Enable[0] == false && conf.CAN_Enable[1] == false && conf.CAN_Enable[2] == false)
            	{
                	Close_CAN_cannel(); // TODO in can work in parralel, LIN

                    if (build_int & 0x80000000) { //signals that enabled and listen only status are also being passed
                    	if ((build_int & 0x60000000) == 0x60000000) {
                    		conf.CAN_Enable[3] = true;
                            eeprom_settings.CAN_mode[3] = CAN_MODE_SILENT;
                            conf.state = SAVVYCAN_CONNECT;
                            eeprom_settings.numBus = 3;
                            Open_CAN_cannel();
                        } else if (build_int & 0x40000000) {
                        	conf.CAN_Enable[3] = true;
                            eeprom_settings.CAN_mode[3] = CAN_MODE_NORMAL;
                            conf.state = SAVVYCAN_CONNECT;
                            eeprom_settings.numBus = 3;
                            Open_CAN_cannel();
                        } else {
                        	conf.CAN_Enable[3] = false;
                            //Close_CAN_cannel();
                        }
                    } else {
                    	conf.state = SAVVYCAN_CONNECT;
                    	eeprom_settings.CAN_mode[3] = CAN_MODE_NORMAL;
                    	eeprom_settings.numBus = 3;
                        Open_CAN_cannel(); //if not using extended status mode then just default to enabling - this was old behavior
                        conf.CAN_Enable[3] = true;
                    }
            	}


            } else { //disable first canbus
                //Close_CAN_cannel();
                conf.CAN_Enable[3] = false;
            }
            break;
        case 8:
            build_int = in_byte;
            break;
        case 9:
            build_int |= in_byte << 8;
            break;
        case 10:
            build_int |= in_byte << 16;
            break;
        case 11:
            build_int |= in_byte << 24;
            state = IDLE;
            //now, write out the new canbus settings to EEPROM
            //EEPROM.write(EEPROM_PAGE, settings);
            break;
        }
        step++;
        break;
    }
}

HAL_StatusTypeDef Open_LIN_cannel()
{
	return HAL_ERROR;
}

HAL_StatusTypeDef Close_LIN_cannel(void)
{
	return HAL_ERROR;
}

HAL_StatusTypeDef Open_CAN_cannel()
{
	Change_CAN_channel();
	if(CAN_Init_Custom(eeprom_settings.CAN_Speed[eeprom_settings.numBus], eeprom_settings.CAN_mode[eeprom_settings.numBus]) == HAL_OK)//);CAN_MODE_LOOPBACK
	{
		EEPROM_Write(&hspi2,
				EEPROM_SETINGS_ADDR + ((uint32_t)&eeprom_settings.CAN_Speed[eeprom_settings.numBus] - (uint32_t)&eeprom_settings),
				(uint8_t*)&eeprom_settings.CAN_Speed[eeprom_settings.numBus], sizeof(eeprom_settings.CAN_Speed[eeprom_settings.numBus]));
		EEPROM_Write(&hspi2,
				EEPROM_SETINGS_ADDR + ((uint32_t)&eeprom_settings.CAN_mode[eeprom_settings.numBus] - (uint32_t)&eeprom_settings),
				(uint8_t*)&eeprom_settings.CAN_mode[eeprom_settings.numBus], sizeof(eeprom_settings.CAN_mode[eeprom_settings.numBus]));

		  //if(SetFilterCAN(0, 0, 0, 0) != HAL_OK) return HAL_ERROR;
		  if(HAL_CAN_Start(&hcan) != HAL_OK) return HAL_ERROR;
		  if(HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING |
				  	  	  	  	  	  	  	  	 CAN_IT_RX_FIFO0_FULL |
												 CAN_IT_RX_FIFO1_MSG_PENDING |
												 CAN_IT_RX_FIFO1_FULL |
												 CAN_IT_TX_MAILBOX_EMPTY |
												 CAN_IT_BUSOFF) != HAL_OK) return HAL_ERROR;

		return HAL_OK;
	}
	return HAL_ERROR;
}

HAL_StatusTypeDef Close_CAN_cannel(void)
{
	if(HAL_CAN_DeInit(&hcan) == HAL_OK)
	{
		HAL_GPIO_WritePin(ERROR_LED_GPIO_Port, ERROR_LED_Pin, GPIO_PIN_RESET);
		return HAL_OK;
	}

	return HAL_ERROR;
}

void Change_CAN_channel(void)
{
	Close_CAN_cannel();
	Close_LIN_cannel();
	HAL_GPIO_WritePin(HS_CAN_EN_GPIO_Port, HS_CAN_EN_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(FT_CAN_EN_GPIO_Port, FT_CAN_EN_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(SW_CAN_EN_GPIO_Port, SW_CAN_EN_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LIN_EN_GPIO_Port, LIN_EN_Pin, GPIO_PIN_RESET);

	//SysSettings.numBus++;
	//if(SysSettings.numBus > 4) SysSettings.numBus = 0;

	HAL_Delay(50);

	switch(eeprom_settings.numBus)
	{
	default: break;
	case 0: HAL_GPIO_WritePin(HS_CAN_EN_GPIO_Port, HS_CAN_EN_Pin, GPIO_PIN_SET); break;
	case 1: HAL_GPIO_WritePin(FT_CAN_EN_GPIO_Port, FT_CAN_EN_Pin, GPIO_PIN_SET); break;
	case 2: HAL_GPIO_WritePin(SW_CAN_EN_GPIO_Port, SW_CAN_EN_Pin, GPIO_PIN_SET); break;
	case 3: HAL_GPIO_WritePin(LIN_EN_GPIO_Port, LIN_EN_Pin, GPIO_PIN_SET); break;
	}
/*	HAL_Delay(10);

	if(SysSettings.CAN_Speed[SysSettings.numBus] != 0)
	{
    	if(SysSettings.numBus == 3) // LIN
    	{
        	Open_LIN_cannel();
    	}
    	else // CAN
    	{
    		Open_CAN_cannel();
    	}
	}*/
	EEPROM_Write(&hspi2,
			EEPROM_SETINGS_ADDR + ((uint32_t)&eeprom_settings.numBus - (uint32_t)&eeprom_settings),
			(uint8_t*)&eeprom_settings.numBus, sizeof(eeprom_settings.numBus));
}

void Next_CAN_channel (void)
{
	eeprom_settings.numBus++;
	if(eeprom_settings.numBus >= 4) eeprom_settings.numBus = 0;
	//Change_CAN_channel();

	Open_CAN_cannel();
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan)
{
	//HAL_GPIO_WritePin(TX_LED_GPIO_Port, TX_LED_Pin, GPIO_PIN_RESET);
}
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan)
{
	//HAL_GPIO_WritePin(TX_LED_GPIO_Port, TX_LED_Pin, GPIO_PIN_RESET);
}
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan)
{
	//HAL_GPIO_WritePin(TX_LED_GPIO_Port, TX_LED_Pin, GPIO_PIN_RESET);
}
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
	HAL_GPIO_WritePin(ERROR_LED_GPIO_Port, ERROR_LED_Pin, GPIO_PIN_SET);
}


#define MAX_BRP 1024
#define MIN_TQ  8
#define MAX_TQ1 16
#define MAX_TQ2 8
#define MAX_TQ  (MAX_TQ1 + MAX_TQ2 + 1) //25

#define SAMPE_POINT 0.875f //0.75f

void STM_bxCAN_calc(uint32_t freq, float bitrate, CAN_HandleTypeDef * hcan)
{
	CAN_HandleTypeDef temp_reg;
    float accuracy, best_accuracy = 1;
    uint32_t brpresc;
    float number_of_time_quanta;
    uint32_t ceil_of_time_quanta;
    float real_sp, best_sp = 0;

    if(bitrate > 4000000.0f || bitrate < 2000.0f) return;

    for(uint32_t i = 0; i < MAX_BRP; i++)
    {
        brpresc = i+1;
        number_of_time_quanta = freq / (bitrate * brpresc);
        ceil_of_time_quanta = roundf(number_of_time_quanta);

        if(ceil_of_time_quanta < MIN_TQ) break;
        if(ceil_of_time_quanta > MAX_TQ) continue;

        accuracy = fabsf(number_of_time_quanta - ceil_of_time_quanta);
        if(accuracy < 0.00001f) accuracy = 0.0f;

        //if(accuracy > 0.5f) continue;

        temp_reg.Init.Prescaler = brpresc;
        temp_reg.Init.TimeSeg1 = (roundf(ceil_of_time_quanta * SAMPE_POINT)) - 1;
        temp_reg.Init.TimeSeg2 = ceil_of_time_quanta - temp_reg.Init.TimeSeg1 - 1;

        if(temp_reg.Init.TimeSeg1 > MAX_TQ1 || temp_reg.Init.TimeSeg2 > MAX_TQ2) continue;

        if(temp_reg.Init.TimeSeg2 < 1 && temp_reg.Init.TimeSeg1 >=1)
        {
            temp_reg.Init.TimeSeg2 +=1;
            temp_reg.Init.TimeSeg1 -=1;
        }

        real_sp = ((float)(temp_reg.Init.TimeSeg1 + 1) / (float)ceil_of_time_quanta);

        //if(real_sp < 0.5f) continue;

        if((accuracy < best_accuracy) || ((accuracy == 0.0f) && (fabsf(real_sp - SAMPE_POINT) < fabsf(best_sp - SAMPE_POINT)) && (temp_reg.Init.TimeSeg2 == hcan->Init.TimeSeg2)))
        {
            hcan->Init.Prescaler = temp_reg.Init.Prescaler;
            hcan->Init.TimeSeg1 = temp_reg.Init.TimeSeg1;
            hcan->Init.TimeSeg2 = temp_reg.Init.TimeSeg2;
            best_accuracy = accuracy;
            best_sp = real_sp;
        }
    }

}
