/*
 * eeprom.h
 *
 *  Created on: 26 θών. 2020 γ.
 *      Author: Arh
 */

#ifndef EEPROM_H_
#define EEPROM_H_

#include <stdint.h>
#include "main.h"
//#include <stm32f1xx_hal_def.h>

//#define EEPROM_Get_Adr(x)

extern SPI_HandleTypeDef hspi2;

HAL_StatusTypeDef EEPROM_Write_disable(SPI_HandleTypeDef *hspi, uint32_t Timeout);
HAL_StatusTypeDef EEPROM_Wait_ready(SPI_HandleTypeDef *hspi, uint32_t Timeout);
HAL_StatusTypeDef EEPROM_Read(SPI_HandleTypeDef *hspi, uint16_t address, uint8_t *pData, uint16_t Size);
HAL_StatusTypeDef EEPROM_Write(SPI_HandleTypeDef *hspi, uint16_t address, uint8_t *pData, uint16_t Size);


#endif /* EEPROM_H_ */
