/*
 * eeprom.c
 *
 *  Created on: 26 θών. 2020 γ.
 *      Author: Arh
 */

//#include <stm32f1xx_hal_def.h>
#include "eeprom.h"
#include "GVRET.h"

#define EEPROM_WREN		0x06
#define EEPROM_WRDI		0x04
#define EEPROM_RDSR		0x05
#define EEPROM_WRSR		0x01
#define EEPROM_READ		0x03
#define EEPROM_WRIRE	0x02


HAL_StatusTypeDef EEPROM_Write_disable(SPI_HandleTypeDef *hspi, uint32_t Timeout)
{
	  //uint32_t tickstart = 0U;
	  HAL_StatusTypeDef errorcode = HAL_OK;
	  uint8_t request[1];
	  //tickstart = HAL_GetTick();
	  request[0] = EEPROM_WRDI;

	  HAL_GPIO_WritePin(EEPROM_CS_GPIO_Port, EEPROM_CS_Pin, GPIO_PIN_RESET);
	  errorcode = HAL_SPI_Transmit(hspi, request, 1, Timeout);
	  HAL_GPIO_WritePin(EEPROM_CS_GPIO_Port, EEPROM_CS_Pin, GPIO_PIN_SET);
	  return errorcode;
}

HAL_StatusTypeDef EEPROM_Wait_ready(SPI_HandleTypeDef *hspi, uint32_t Timeout)
{
	  uint32_t tickstart = 0U;
	  HAL_StatusTypeDef errorcode = HAL_OK;
	  uint8_t request[1];
	  tickstart = HAL_GetTick();
	  request[0] = EEPROM_RDSR;

	  HAL_GPIO_WritePin(EEPROM_CS_GPIO_Port, EEPROM_CS_Pin, GPIO_PIN_RESET);
	  errorcode = HAL_SPI_Transmit(hspi, request, 1, Timeout);

	  if(errorcode != HAL_OK)
	  {
	  	HAL_GPIO_WritePin(EEPROM_CS_GPIO_Port, EEPROM_CS_Pin, GPIO_PIN_SET);
	  	return errorcode;
	  }

	  do{
		errorcode = HAL_SPI_Receive(hspi, request, 1, Timeout);
		if(errorcode == HAL_BUSY || ((HAL_GetTick()-tickstart) >=  Timeout))
		{
			return HAL_BUSY;
		}
	  }while(request[0] & 0x03);

	  HAL_GPIO_WritePin(EEPROM_CS_GPIO_Port, EEPROM_CS_Pin, GPIO_PIN_SET);
	  return HAL_OK;
}



HAL_StatusTypeDef EEPROM_Read(SPI_HandleTypeDef *hspi, uint16_t address, uint8_t *pData, uint16_t Size)
{
	HAL_StatusTypeDef errorcode = HAL_OK;
	uint8_t request[3];
	request[0] = EEPROM_READ;
	request[1] = address >> 8;
	request[2] = address & 0xFF;

	errorcode = EEPROM_Wait_ready(&hspi2, 500);
	if(errorcode != HAL_OK) return errorcode;

	HAL_GPIO_WritePin(EEPROM_CS_GPIO_Port, EEPROM_CS_Pin, GPIO_PIN_RESET);
	errorcode = HAL_SPI_Transmit(hspi, request, 3, 100);
	if(errorcode != HAL_OK)
	{
		HAL_GPIO_WritePin(EEPROM_CS_GPIO_Port, EEPROM_CS_Pin, GPIO_PIN_SET);
		return errorcode;
	}

	errorcode = HAL_SPI_Receive(hspi, pData, Size, 100);
	HAL_GPIO_WritePin(EEPROM_CS_GPIO_Port, EEPROM_CS_Pin, GPIO_PIN_SET);
	return errorcode;
}


HAL_StatusTypeDef EEPROM_Write(SPI_HandleTypeDef *hspi, uint16_t address, uint8_t *pData, uint16_t Size)
{
	HAL_StatusTypeDef errorcode = HAL_OK;
	uint8_t request[35];
	volatile uint16_t sended_bytes = 0;
	volatile uint8_t address_in_page = 0;

	do
	{
		errorcode = EEPROM_Wait_ready(&hspi2, 500);
		if(errorcode != HAL_OK) return errorcode;

		request[0] = EEPROM_WREN;

		HAL_GPIO_WritePin(EEPROM_CS_GPIO_Port, EEPROM_CS_Pin, GPIO_PIN_RESET);
		errorcode = HAL_SPI_Transmit(hspi, request, 1, 100);
		HAL_GPIO_WritePin(EEPROM_CS_GPIO_Port, EEPROM_CS_Pin, GPIO_PIN_SET);
		if(errorcode != HAL_OK) return errorcode;

		address_in_page = 0;

		request[0] = EEPROM_WRIRE;
		request[1] = (address + sended_bytes) >> 8;
		request[2] = (address + sended_bytes) & 0xFF;

		while((address_in_page < 32) && (sended_bytes < Size))
		{
			request[3 + address_in_page++] = pData[sended_bytes++];
			if((address + sended_bytes) % 32 == 0) break;
		}
		HAL_GPIO_WritePin(EEPROM_CS_GPIO_Port, EEPROM_CS_Pin, GPIO_PIN_RESET);
		errorcode = HAL_SPI_Transmit(hspi, request, 3 + address_in_page, 100);
		HAL_GPIO_WritePin(EEPROM_CS_GPIO_Port, EEPROM_CS_Pin, GPIO_PIN_SET);
		if(errorcode != HAL_OK) return errorcode;
	}
	while(sended_bytes < Size);

	return errorcode;
}

