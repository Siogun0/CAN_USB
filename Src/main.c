/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "dma.h"
#include "rtc.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
//#include "usbd_cdc_if.h"
#include <stdlib.h>
#include "button.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
UART_HandleTypeDef *huart_active;
UART_HandleTypeDef *huart_lin;
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint64_t time_stamp_UART = 0;
uint64_t time_stamp_LED = 0;
uint64_t time_stamp_BUT = 0;
can_msg_t can_msg;
uint8_t uart_tx_bufer[128];
uint8_t uart_tx_bufer_1[128];
uint8_t uart_tx_bufer_active_no = 0;
uint8_t uart_tx_com_bufer[32];
uint8_t uart_answ_ready = 0;
uint32_t uart_tx_pointer = 0;
uint8_t uart_rx_bufer[1024];
uint8_t uart_rx_char;


uint8_t debug_buf[1024];
uint32_t debug_pt = 0;


conf_t conf;

can_msg_t can_rx_buf[32];
volatile uint8_t uart_busy = 0;

uint8_t test_str[] = "Hello World \r\n";
t_button_var button_var;

uint32_t rx_led_ts = 0;
uint32_t tx_led_ts = 0;
uint32_t error_led_ts = 0;
uint8_t rx_led_z1 = 0;
uint8_t tx_led_z1 = 0;
uint8_t error_led_z1 = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void ConvertValToString(int64_t n, uint8_t s[]);
void SendToUart_CanHacker(can_msg_t msg);

uint8_t exec_usb_cmd (uint8_t * cmd_buf);
void UART_Check_Data_Ready(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t txt_hallo[] = "CAN temperature logger\r\n";
uint8_t timestamp[] = "1234567890";

void UART_Check_Data_Ready(void)
{
	if(uart_answ_ready == 1)
	{
		for(int i = 0; i < uart_tx_pointer; i++)
		{
			uart_tx_bufer[i] = uart_tx_com_bufer[i];
		}
		HAL_UART_Transmit_DMA(huart_active, uart_tx_bufer, uart_tx_pointer);
		uart_tx_pointer = 0;
		uart_busy = 1;
		uart_answ_ready = 0;
	}
	else
	{
		if(conf.state == CAN_HACKER_CONNECT || conf.state == SAVVYCAN_CONNECT)
		{
			if(CAN_Buffer_pull() == HAL_OK)
			{
				uart_busy = 1;
				//HAL_GPIO_WritePin(RX_LED_GPIO_Port, RX_LED_Pin, GPIO_PIN_RESET);
			}
		}

	}

}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	uart_busy = 0;
	UART_Check_Data_Ready();
	//HAL_GPIO_WritePin(RX_LED_GPIO_Port, RX_LED_Pin, GPIO_PIN_RESET);
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO1, &can_rx_buf[0].header, can_rx_buf[1].data_byte);
	can_rx_buf[1].timestamp = HAL_GetTick();
	CAN_Buffer_Write_Data(can_rx_buf[1]);
	HAL_GPIO_WritePin(RX_LED_GPIO_Port, RX_LED_Pin, GPIO_PIN_SET);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &can_rx_buf[0].header, can_rx_buf[0].data_byte);
	can_rx_buf[0].timestamp = HAL_GetTick();
	CAN_Buffer_Write_Data(can_rx_buf[0]);
	HAL_GPIO_WritePin(RX_LED_GPIO_Port, RX_LED_Pin, GPIO_PIN_SET);
}

#define EEPROM_WREN		0x06
#define EEPROM_WRDI		0x04
#define EEPROM_RDSR		0x05
#define EEPROM_WRSR		0x01
#define EEPROM_READ		0x03
#define EEPROM_WRIRE	0x02


HAL_StatusTypeDef EEPROM_Read(SPI_HandleTypeDef *hspi, uint16_t address, uint8_t *pData, uint16_t Size)
{
	HAL_StatusTypeDef errorcode = HAL_OK;
	uint8_t request[3];
	request[0] = EEPROM_READ;
	request[1] = address >> 8;
	request[2] = address & 0xFF;

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
	uint8_t request[3];
	request[0] = EEPROM_WREN;

	HAL_GPIO_WritePin(EEPROM_CS_GPIO_Port, EEPROM_CS_Pin, GPIO_PIN_RESET);
	errorcode = HAL_SPI_Transmit(hspi, request, 4, 100);
	HAL_GPIO_WritePin(EEPROM_CS_GPIO_Port, EEPROM_CS_Pin, GPIO_PIN_SET);
	if(errorcode != HAL_OK) return errorcode;

	request[0] = EEPROM_WRIRE;
	request[1] = address >> 8;
	request[2] = address & 0xFF;
	HAL_GPIO_WritePin(EEPROM_CS_GPIO_Port, EEPROM_CS_Pin, GPIO_PIN_RESET);
	errorcode = HAL_SPI_Transmit(hspi, request, 4, 100);
	if(errorcode != HAL_OK)
	{
		HAL_GPIO_WritePin(EEPROM_CS_GPIO_Port, EEPROM_CS_Pin, GPIO_PIN_SET);
		return errorcode;
	}

	errorcode = HAL_SPI_Transmit(hspi, pData, Size, 100);
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
	  }while(request[0]);

	  return HAL_OK;
}



/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	CAN_Buffer_Init();
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  huart_active = &huart3;
  huart_lin = &huart1;
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_RTC_Init();
  MX_USART3_UART_Init();
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  MX_CAN_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(100);


  //EEPROM_Wait_ready(&hspi2, 50000);
//  EEPROM_Write(&hspi2, 0, test_str, sizeof(test_str));
  HAL_Delay(100);
//  EEPROM_Read(&hspi2, 0, (uint8_t*)&eeprom_settings, sizeof(eeprom_settings));


  if(HAL_GPIO_ReadPin(BOOT1_GPIO_Port, BOOT1_Pin) == 1)
  {
	  huart3.Init.BaudRate = 115200;
  }
  else
  {
	  huart3.Init.BaudRate = (eeprom_settings.UART_Speed > 0 && eeprom_settings.UART_Speed < 10000000) ? eeprom_settings.UART_Speed : 115200;
  }
  HAL_UART_Init(&huart3);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  //SendToUart();
  //HAL_UART_Receive_DMA(&huart1, uart_rx_bufer, 2);
  while (1)
  {

	  //CDC_Transmit_FS(test_str, 5);
	  //HAL_Delay(100);


	  if(USART3->SR & USART_SR_RXNE) // TODO Сделать фоновый прием данных USART3
	  {
		  uart_rx_char = USART3->DR;
		  debug_buf[debug_pt++] = uart_rx_char;
		  if(debug_pt == 1024) debug_pt = 0;
		  Check_Command(uart_rx_char);

	  }

		if(HAL_GetTick() - time_stamp_UART >= 20 && uart_busy == 0)
		{
			time_stamp_UART = HAL_GetTick();
			UART_Check_Data_Ready();
		}

		// LED blinking
		if(HAL_GetTick() - time_stamp_LED >= 500)
		{
			time_stamp_LED = HAL_GetTick();
			HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
		}


		if(Button_rutine(&button_var, HAL_GPIO_ReadPin(BUTTON_GPIO_Port, BUTTON_Pin) == 1) == DOUBLE_CLICKED)
		{
			Next_CAN_channel();
		}

		if(HAL_GPIO_ReadPin(RX_LED_GPIO_Port, RX_LED_Pin) == GPIO_PIN_SET)
		{
			if(rx_led_z1 == 0)
			{
				rx_led_z1 = 1;
				rx_led_ts = HAL_GetTick();
			}
			else if(HAL_GetTick() - rx_led_ts > 3)
			{
				HAL_GPIO_WritePin(RX_LED_GPIO_Port, RX_LED_Pin, GPIO_PIN_RESET);
				rx_led_z1 = 0;
				rx_led_ts = HAL_GetTick();
			}
		}

		if(HAL_GPIO_ReadPin(TX_LED_GPIO_Port, TX_LED_Pin) == GPIO_PIN_SET)
		{
			if(tx_led_z1 == 0)
			{
				tx_led_z1 = 1;
				tx_led_ts = HAL_GetTick();
			}
			else if(HAL_GetTick() - tx_led_ts > 3)
			{
				HAL_GPIO_WritePin(TX_LED_GPIO_Port, TX_LED_Pin, GPIO_PIN_RESET);
				tx_led_z1 = 0;
			}
		}

		if(HAL_GPIO_ReadPin(ERROR_LED_GPIO_Port, ERROR_LED_Pin) == GPIO_PIN_SET)
		{
			if(HAL_CAN_GetError(&hcan) == HAL_CAN_ERROR_BOF) break;
			if(error_led_z1 == 0)
			{
				error_led_z1 = 1;
				error_led_ts = HAL_GetTick();
			}
			else if(HAL_GetTick() - error_led_ts > 100)
			{
				HAL_GPIO_WritePin(ERROR_LED_GPIO_Port, ERROR_LED_Pin, GPIO_PIN_RESET);
				error_led_z1 = 0;
			}
		}

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/* itoa:  convert integer to char[] */
void ConvertValToString(int64_t n, uint8_t s[])
{
    int i;
	   int64_t sign;

    if ((sign = n) < 0)  /* ?????????? ???? */
        n = -n;          /* ?????? n ????????????? ?????? */
    i = 0;
    do {       /* ?????????? ????? ? ???????? ??????? */
        s[i++] = n % 10 + '0';   /* ????? ????????? ????? */
    } while ((n /= 10) > 0);     /* ??????? */
    if (sign < 0)
        s[i++] = '-';
    for(int j = 0; j < i/2; j++)
    {
    	uint8_t buf = s[i - j - 1];
    	s[i - j - 1] = s[j];
    	s[j] = buf;
    }
    s[i] = '\0';

}





/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
