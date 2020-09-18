/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "GVRET.h"
#include "ff.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
extern UART_HandleTypeDef *huart_active;
extern UART_HandleTypeDef *huart_lin;

extern uint8_t uart_rx_bufer[1024];
extern uint32_t uart_rx_pointer_w;
extern uint32_t uart_rx_pointer_r;
extern uint8_t uart_rx_char;


/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void Check_Command(uint8_t uart_rx_char);
HAL_StatusTypeDef Save_to_File(uint8_t * buf, uint32_t len);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC
#define SPI1_CS_Pin GPIO_PIN_4
#define SPI1_CS_GPIO_Port GPIOA
#define MODE_SEL_Pin GPIO_PIN_0
#define MODE_SEL_GPIO_Port GPIOB
#define BUTTON_Pin GPIO_PIN_1
#define BUTTON_GPIO_Port GPIOB
#define BOOT1_Pin GPIO_PIN_2
#define BOOT1_GPIO_Port GPIOB
#define USART3_TX_Pin GPIO_PIN_10
#define USART3_TX_GPIO_Port GPIOB
#define USART3_RX_Pin GPIO_PIN_11
#define USART3_RX_GPIO_Port GPIOB
#define EEPROM_CS_Pin GPIO_PIN_12
#define EEPROM_CS_GPIO_Port GPIOB
#define ERROR_LED_Pin GPIO_PIN_8
#define ERROR_LED_GPIO_Port GPIOA
#define LIN_TX_Pin GPIO_PIN_9
#define LIN_TX_GPIO_Port GPIOA
#define LIN_RX_Pin GPIO_PIN_10
#define LIN_RX_GPIO_Port GPIOA
#define TX_LED_Pin GPIO_PIN_15
#define TX_LED_GPIO_Port GPIOA
#define RX_LED_Pin GPIO_PIN_3
#define RX_LED_GPIO_Port GPIOB
#define LIN_EN_Pin GPIO_PIN_4
#define LIN_EN_GPIO_Port GPIOB
#define SW_CAN_EN_Pin GPIO_PIN_5
#define SW_CAN_EN_GPIO_Port GPIOB
#define FT_CAN_EN_Pin GPIO_PIN_6
#define FT_CAN_EN_GPIO_Port GPIOB
#define HS_CAN_EN_Pin GPIO_PIN_7
#define HS_CAN_EN_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

#define EEPROM_VERSION 1
#define EEPROM_SIZE	0x2000
#define EEPROM_SETINGS_ADDR	0x00
#define EEPROM_SCRIPT_ADDR	0x100

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
