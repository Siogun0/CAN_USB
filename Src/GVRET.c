/*
 * GVRET.c
 *
 *  Created on: 5 мар. 2020 г.
 *      Author: Arh
 */
#include "GVRET.h"
#include "can.h"
#include <string.h>
#include <math.h>

EEPROMSettings settings;
SystemSettings SysSettings;
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
} CAN_Buffer;

void CAN_Buffer_Init(void)
{
	CAN_Buffer.Pointer_Read = 0;
	CAN_Buffer.Pointer_Write = 0;
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

    if (SysSettings.lawicelMode)
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

    	if(conf.timestamp_en || SysSettings.lawicelTimestamping == true)
    	{
    		uint16_t timestamp = frame.timestamp % 60000u;
    		ShortToHex(timestamp >> 8 , &buf[pointer]);
    		pointer +=2;
    		ShortToHex(timestamp & 0xFF , &buf[pointer]);
    		pointer +=2;
    	}

    	buf[pointer++] = CR;
    }
    else
    {
        if (settings.useBinarySerialComm)
        {
        	uint32_t id_temp;
            if (frame.header.IDE == CAN_ID_EXT) id_temp = frame.header.ExtId | 1 << 31;
            else id_temp = frame.header.StdId;
            buf[pointer++] = 0xF1;
            buf[pointer++] = 0; //0 = canbus frame sending
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
            //SerialUSB.write(buff, 12 + frame.length);
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

HAL_StatusTypeDef SetFilterCAN(uint32_t id, uint32_t mask)
{
	 if(mask == 0)
	 {
 		  CAN_FilterTypeDef CAN_FilterStructure;
 		  CAN_FilterStructure.FilterBank = 1;
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
	 }
	 else
	 {

	 }
	 return HAL_OK;
}

uint8_t exec_usb_cmd (uint8_t * cmd_buf)
{
    uint8_t cmd_len = strlen ((char *)cmd_buf);	// get command length


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
            	conf.timestamp_en = 1;
            	SysSettings.lawicelTimestamping = true;
            }
            else if(cmd_buf[1] == '0')
            {
            	conf.timestamp_en = 0;
            	SysSettings.lawicelTimestamping = false;
            }
            else
            {
            	conf.timestamp_en = (conf.timestamp_en == 1) ? 0 : 1;
            	SysSettings.lawicelTimestamping = (SysSettings.lawicelTimestamping) ? false : true;
            }
            return CR;

            // read status flag
        case READ_STATUS:

        	uart_tx_com_bufer[uart_tx_pointer++] = (READ_STATUS);
           // usb_byte2ascii ((uint8_t) (CAN_flags >> 8));
            uart_tx_com_bufer[uart_tx_pointer++] = '0';
            uart_tx_com_bufer[uart_tx_pointer++] = HAL_CAN_GetState(&hcan) + '0';
            // turn off Bus Error indication
            //LED_PORT &= ~_BV (LED_RED);
            // reset error flags
            HAL_CAN_ResetError(&hcan);
            return CR;

            // set AMR
        case SET_AMR:
            // set ACR
        case SET_ACR:
            return CR;

            // set bitrate via BTR
        case SET_CUSTOM_BTR:
        	SysSettings.CAN_Speed[SysSettings.numBus] = 0;
        	for(int i = 1; i < cmd_len; i++)
        	{
        		if(cmd_buf[i] < '0' || cmd_buf[i] > '9') return ERROR;
        		SysSettings.CAN_Speed[SysSettings.numBus] *= 10;
        		SysSettings.CAN_Speed[SysSettings.numBus] += HexTo4bits(cmd_buf[i]);
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
            	SysSettings.CAN_Speed[SysSettings.numBus] = 10000;
            	break;
            case '1':
            	SysSettings.CAN_Speed[SysSettings.numBus] = 20000;
            	break;
            case '2':
            	SysSettings.CAN_Speed[SysSettings.numBus] = 50000;
            	break;
            case '3':
            	SysSettings.CAN_Speed[SysSettings.numBus] = 100000;
            	break;
            case '4':
            	SysSettings.CAN_Speed[SysSettings.numBus] = 125000;
            	break;
            case '5':
            	SysSettings.CAN_Speed[SysSettings.numBus] = 250000;
            	break;
            case '6':
            	SysSettings.CAN_Speed[SysSettings.numBus] = 500000;
            	break;
            case '7':
            	SysSettings.CAN_Speed[SysSettings.numBus] = 800000;
            	break;
            case '8':
            	SysSettings.CAN_Speed[SysSettings.numBus] = 1000000;
            	break;
            case '9':
            	SysSettings.CAN_Speed[SysSettings.numBus] = 95238;
            	break;
            case 'a':
            case 'A':
            	SysSettings.CAN_Speed[SysSettings.numBus] = 8333;
            	break;
            case 'b':
            case 'B':
            	SysSettings.CAN_Speed[SysSettings.numBus] = 47619;
            	break;
            case 'c':
            case 'C':
            	SysSettings.CAN_Speed[SysSettings.numBus] = 33333;
            	break;
            case 'd':
            case 'D':
            	SysSettings.CAN_Speed[SysSettings.numBus] = 5000;
            	break;
            default:
            	return ERROR;
            	break;
            }
            return CR;


            // open CAN channel
        case OPEN_CAN_CHAN:

            // return error if controller is not initialized or already open
        	if(SysSettings.numBus == 3) // LIN
        	{
            	SysSettings.CAN_mode[SysSettings.numBus] = CAN_MODE_NORMAL;
            	if(Open_LIN_cannel() != HAL_OK) return ERROR;
        	}
        	else // CAN
        	{
                if (HAL_CAN_GetState(&hcan) != HAL_CAN_STATE_RESET) return ERROR;

            	SysSettings.CAN_mode[SysSettings.numBus] = CAN_MODE_NORMAL;
            	if(Open_CAN_cannel() != HAL_OK) return ERROR;
        	}


    		conf.state = CAN_HACKER_CONNECT;
    		SysSettings.lawicelMode = true;
            return CR;

            // close CAN channel

        case CLOSE_CAN_CHAN:
        	if(SysSettings.numBus == 3) // LIN
        	{
            	if(Close_LIN_cannel() != HAL_OK) return ERROR;
        	}
        	else // CAN
        	{
        		HAL_CAN_ResetError(&hcan);
        		if(Close_CAN_cannel() != HAL_OK) return ERROR;
        	}
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
        	SysSettings.CAN_mode[SysSettings.numBus] = CAN_MODE_SILENT;
        	if(Open_CAN_cannel() != HAL_OK) return ERROR;
    		conf.state = CAN_HACKER_CONNECT;
    		SysSettings.lawicelMode = true;
            return CR;

            //Set USART bitrate
        case SET_USART_BTR:
        	switch (cmd_buf[1])
        	{
        	case '0': uart_speed = 230400; break;
        	case '1': uart_speed = 115200; break;
        	case '2': uart_speed = 57600; break;
        	case '3': uart_speed = 38400; break;
        	case '4': uart_speed = 19200; break;
        	case '5': uart_speed = 9600; break;
        	case '6': uart_speed = 2400; break;
        	default: uart_speed = 1000000; break;
        	}
        	HAL_UART_DeInit(huart_active);
        	huart3.Init.BaudRate = uart_speed;
        	if (HAL_UART_Init(huart_active) != HAL_OK)
        	{
        		return ERROR;
        	}

        	return CR; // set predefined speed

        case SET_USART_BTR_CUST:
        	uart_speed = 0;
        	for(int i = 1; i < cmd_len; i++)
        	{
        		if(cmd_buf[i] < '0' || cmd_buf[i] > '9') return ERROR;
        		uart_speed *= 10;
        		uart_speed += HexTo4bits(cmd_buf[i]);
        	}
        	if(uart_speed < 200 || uart_speed > 2000000) return ERROR;

        	HAL_UART_DeInit(huart_active);
        	huart3.Init.BaudRate = uart_speed;
        	if (HAL_UART_Init(huart_active) != HAL_OK)
        	{
        		return ERROR;
        	}

        	return CR; // set custom speed

        	// Select Channel
        case SELECT_BUS_CHANEL:
        	if(cmd_buf[1] < '0' || cmd_buf[1] > '4') return ERROR;
        	SysSettings.numBus = HexTo4bits(cmd_buf[1]) - 1;
        	Change_CAN_channel();
        	return CR;


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
            settings.useBinarySerialComm = true;
            SysSettings.lawicelMode = false;
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

        	// TODO debug initialisation
        	SysSettings.CAN_Speed[0] = 500000;
        	SysSettings.CAN_Speed[1] = 95238;
        	SysSettings.CAN_Speed[2] = 33333;
        	SysSettings.CAN_Speed[3] = 9600;

        	uart_tx_com_bufer[uart_tx_pointer++] = 0xF1;
        	uart_tx_com_bufer[uart_tx_pointer++] = 6;
        	uart_tx_com_bufer[uart_tx_pointer++] = settings.CAN0_Enabled + ((unsigned char)settings.CAN0ListenOnly << 4);
        	uart_tx_com_bufer[uart_tx_pointer++] = SysSettings.CAN_Speed[0];
        	uart_tx_com_bufer[uart_tx_pointer++] = SysSettings.CAN_Speed[0] >> 8;
        	uart_tx_com_bufer[uart_tx_pointer++] = SysSettings.CAN_Speed[0] >> 16;
        	uart_tx_com_bufer[uart_tx_pointer++] = SysSettings.CAN_Speed[0] >> 24;
        	uart_tx_com_bufer[uart_tx_pointer++] = settings.CAN1_Enabled + ((unsigned char)settings.CAN1ListenOnly << 4) + ((unsigned char)settings.singleWire_Enabled << 6);
        	uart_tx_com_bufer[uart_tx_pointer++] = SysSettings.CAN_Speed[1];
        	uart_tx_com_bufer[uart_tx_pointer++] = SysSettings.CAN_Speed[1] >> 8;
        	uart_tx_com_bufer[uart_tx_pointer++] = SysSettings.CAN_Speed[1] >> 16;
        	uart_tx_com_bufer[uart_tx_pointer++] = SysSettings.CAN_Speed[1] >> 24;
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
        	uart_tx_com_bufer[uart_tx_pointer++] = (unsigned char)settings.fileOutputType;
        	uart_tx_com_bufer[uart_tx_pointer++] = (unsigned char)settings.autoStartLogging;
        	uart_tx_com_bufer[uart_tx_pointer++] = settings.singleWire_Enabled;
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
            uart_tx_com_bufer[uart_tx_pointer++] = 4; //CAN0, CAN1, SWCAN
            uart_answ_ready = 1;
            state = IDLE;
            break;
         case PROTO_GET_EXT_BUSES:
            uart_tx_com_bufer[0] = 0xF1;
            uart_tx_com_bufer[1] = 13;
            uart_tx_com_bufer[2] = settings.singleWire_Enabled + ((unsigned char)settings.SWCANListenOnly << 4);
            uart_tx_com_bufer[3] = SysSettings.CAN_Speed[2];
            uart_tx_com_bufer[4] = SysSettings.CAN_Speed[2] >> 8;
            uart_tx_com_bufer[5] = SysSettings.CAN_Speed[2] >> 16;
            uart_tx_com_bufer[6] = SysSettings.CAN_Speed[2] >> 24;
            uart_tx_com_bufer[7] = 0; //fourth bus enabled
            uart_tx_com_bufer[8] = SysSettings.CAN_Speed[3]; //fourth bus speed (4 bytes)
            uart_tx_com_bufer[9] = SysSettings.CAN_Speed[3] >> 8;
            uart_tx_com_bufer[10] = SysSettings.CAN_Speed[3] >> 16;
            uart_tx_com_bufer[11] = SysSettings.CAN_Speed[3] >> 24;
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
            	SysSettings.CAN_Speed[SysSettings.numBus] = build_int & 0xFFFFF;
                if (build_int & 0x80000000) { //signals that enabled and listen only status are also being passed
                	if ((build_int & 0x60000000) == 0x60000000) {
                        settings.CAN0ListenOnly = true;
                        SysSettings.CAN_mode[SysSettings.numBus] = CAN_MODE_SILENT;
                        conf.state = SAVVYCAN_CONNECT;
                        Open_CAN_cannel();
                    } else if (build_int & 0x40000000) {
                        settings.CAN0_Enabled = true;
                        SysSettings.CAN_mode[SysSettings.numBus] = CAN_MODE_NORMAL;
                        conf.state = SAVVYCAN_CONNECT;
                        Open_CAN_cannel();
                    } else {
                        settings.CAN0_Enabled = false;
                        Close_CAN_cannel();
                    }
                } else {
                	conf.state = SAVVYCAN_CONNECT;
                	SysSettings.CAN_mode[SysSettings.numBus] = CAN_MODE_NORMAL;
                    Open_CAN_cannel(); //if not using extended status mode then just default to enabling - this was old behavior
                    settings.CAN0_Enabled = true;
                }
                build_int = build_int & 0xFFFFF;
                //if (build_int > 1000000) build_int = 1000000;

                //Can0.begin(build_int, SysSettings.CAN0EnablePin);
                //Can0.set_baudrate(build_int);
                settings.CAN0Speed = build_int;
            } else { //disable first canbus
                Close_CAN_cannel();
                settings.CAN0_Enabled = false;
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
            if (build_int > 0) {
                if (build_int & 0x80000000) { //signals that enabled and listen only status are also being passed
                    if (build_int & 0x40000000) {
                        settings.CAN1_Enabled = true;
                        //Can1.enable();
                    } else {
                        settings.CAN1_Enabled = false;
                        //Can1.disable();
                    }
                    if (build_int & 0x20000000) {
                        settings.CAN1ListenOnly = true;
                        //Can1.enable_autobaud_listen_mode();
                    } else {
                        settings.CAN1ListenOnly = false;
                        //Can1.disable_autobaud_listen_mode();
                    }
                } else {
                    //Can1.enable(); //if not using extended status mode then just default to enabling - this was old behavior
                    settings.CAN1_Enabled = true;
                }
                build_int = build_int & 0xFFFFF;
                if (build_int > 1000000) build_int = 1000000;
                //Can1.begin(build_int, SysSettings.CAN1EnablePin);
                //if (settings.singleWire_Enabled && !SysSettings.dedicatedSWCAN) setSWCANEnabled();
                //else setSWCANSleep();
                //Can1.set_baudrate(build_int);

                settings.CAN1Speed = build_int;
            } else { //disable second canbus
                //if (!SysSettings.dedicatedSWCAN) setSWCANSleep();
                //Can1.disable();
                settings.CAN1_Enabled = false;
            }
            state = IDLE;
            //now, write out the new canbus settings to EEPROM
            //EEPROM.write(EEPROM_PAGE, settings);
            setPromiscuousMode();
            break;
        }
        step++;
        break;
    case SET_SINGLEWIRE_MODE:
        if (in_byte == 0x10) {
            settings.singleWire_Enabled = true;
            //setSWCANEnabled();
        } else {
            settings.singleWire_Enabled = false;
            //setSWCANSleep();
        }
        //EEPROM.write(EEPROM_PAGE, settings);
        state = IDLE;
        break;
    case SET_SYSTYPE:
        settings.sysType = in_byte;
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
         /*   if (build_int > 0) {
                if (build_int & 0x80000000) { //signals that enabled and listen only status are also being passed
                    if (build_int & 0x40000000) {
                        settings.singleWire_Enabled = true;
                        setSWCANEnabled();
                    } else {
                        settings.singleWire_Enabled = false;
                        setSWCANSleep();
                    }
                    if (build_int & 0x20000000) {
                        settings.SWCANListenOnly = true;
                        //SWCAN.enable_autobaud_listen_mode();
                    } else {
                        settings.SWCANListenOnly = false;
                        //SWCAN.disable_autobaud_listen_mode();
                    }
                } else {
                    setSWCANEnabled();
                    settings.singleWire_Enabled = true;
                }
                build_int = build_int & 0xFFFFF;
                if (build_int > 100000) build_int = 100000;
                settings.SWCANSpeed = build_int;
                SPI.begin();
                if(SWCAN.Init(settings.SWCANSpeed,16))
                {
                    SerialUSB.println("MCP2515 Init OK ...");
                    attachInterrupt(CANDUE22_SW_INT, SWCAN_Int, FALLING);
                    setSWCANEnabled();
                } else {
                    SerialUSB.println("MCP2515 Init Failed ...");
                }
            } else { //disable first canbus
                void setSWCANSleep();
                settings.singleWire_Enabled = false;
            }*/
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
	if(CAN_Init_Custom(SysSettings.CAN_Speed[SysSettings.numBus], SysSettings.CAN_mode[SysSettings.numBus]) == HAL_OK)//);CAN_MODE_LOOPBACK
	{

	  if(SetFilterCAN(0, 0) != HAL_OK) return HAL_ERROR;
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
		conf.state = IDLE_ST;
		SysSettings.lawicelMode = false;
		HAL_GPIO_WritePin(ERROR_LED_GPIO_Port, ERROR_LED_Pin, GPIO_PIN_RESET);
		return HAL_OK;
	}

	return HAL_ERROR;
}

void Change_CAN_channel(void)
{
	//Close_CAN_cannel();
	//Close_LIN_cannel();
	HAL_GPIO_WritePin(HS_CAN_EN_GPIO_Port, HS_CAN_EN_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(FT_CAN_EN_GPIO_Port, FT_CAN_EN_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(SW_CAN_EN_GPIO_Port, SW_CAN_EN_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LIN_EN_GPIO_Port, LIN_EN_Pin, GPIO_PIN_RESET);

	//SysSettings.numBus++;
	//if(SysSettings.numBus > 4) SysSettings.numBus = 0;

	HAL_Delay(50);

	switch(SysSettings.numBus)
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
}

void Next_CAN_channel (void)
{
	SysSettings.numBus++;
	if(SysSettings.numBus >= 4) SysSettings.numBus = 0;
	Change_CAN_channel();
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
