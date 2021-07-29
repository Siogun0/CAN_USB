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
#include "fatfs.h"
#include "rtc.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
//#include "usbd_cdc_if.h"
#include <stdlib.h>
#include "button.h"
#include "GVRET.h"
#include "eeprom.h"
#include <string.h>
#include "LIN.h"
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
uint64_t time_stamp_SAVE = 0;
//can_msg_t can_msg;
uint8_t uart_tx_bufer[128];
uint8_t uart_tx_bufer_1[128];
uint8_t uart_tx_bufer_active_no = 0;
uint8_t uart_tx_com_bufer[32];
uint8_t uart_answ_ready = 0;
uint32_t uart_tx_pointer = 0;
uint8_t uart_rx_bufer[1024];
uint32_t uart_rx_pointer_w = 0;
uint32_t uart_rx_pointer_r = 0;
uint8_t uart_rx_char;
uint8_t flash_buffer[128];
uint8_t filename[] = LOG_NAME;
uint8_t pathname[] = LOG_PATH;

uint8_t script_read_buf[1024];
uint32_t debug_pt = 0;


conf_t conf;

can_msg_t can_rx_buf[2];
volatile uint8_t uart_busy = 0;

uint8_t test_str[] = "Hello World \r\n";
t_button_var button_var;

uint32_t rx_led_ts = 0;
uint32_t tx_led_ts = 0;
uint32_t error_led_ts = 0;
uint8_t rx_led_z1 = 0;
uint8_t tx_led_z1 = 0;
uint8_t error_led_z1 = 0;

//uint8_t in[100], out[150];

uint8_t script_buf[CMD_BUFFER_LENGTH];
uint8_t script_buf_pointer = 0;

extern const char help_text[];

FRESULT fresult = FR_OK;
FATFS fs;
FIL fil;
extern Disk_drvTypeDef  disk;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void ConvertValToString(int64_t n, uint8_t s[]);
void SendToUart_CanHacker(can_msg_t msg);


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
		if(conf.state == LAWICEL_CONNECT || conf.state == SAVVYCAN_CONNECT)
		{
			if(CAN_Buffer_pull() == HAL_OK)
			{
				uart_busy = 1;
				//HAL_GPIO_WritePin(RX_LED_GPIO_Port, RX_LED_Pin, GPIO_PIN_RESET);
			}
		}
		else
		{
			//CAN_Buffer_clean();
		}

		if(conf.script_print)
		{
			uart_tx_pointer = 0;
			for(conf.script_address = eeprom_settings.start_address_csript; conf.script_address < (eeprom_settings.start_address_csript + 128); conf.script_address++)
			{
				EEPROM_Read(&hspi2, conf.script_address, &uart_tx_bufer[uart_tx_pointer++], 1);
				if(uart_tx_bufer[uart_tx_pointer-1] == 0xFF)
				{
					uart_tx_pointer--;
					conf.script_print = false;
					break;
				}
			}
			HAL_UART_Transmit_DMA(huart_active, uart_tx_bufer, uart_tx_pointer);
			uart_tx_pointer = 0;
			uart_busy = 1;
		}
		if(conf.help_print)
				{
					uart_tx_pointer = 0;
					for(int i = conf.help_text_pointer; i < conf.help_text_pointer + 128; i++)
					{
						uart_tx_bufer[uart_tx_pointer++] = help_text[i];
						if(help_text[i] == '\0')
						{
							uart_tx_pointer--;
							conf.help_print = false;
							break;
						}
					}
					conf.help_text_pointer += 128;
					HAL_UART_Transmit_DMA(huart_active, uart_tx_bufer, uart_tx_pointer);
					uart_tx_pointer = 0;
					uart_busy = 1;
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
	HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO1, &can_rx_buf[1].header, can_rx_buf[1].data_byte);
	can_rx_buf[1].timestamp = HAL_GetTick();
	can_rx_buf[1].bus = eeprom_settings.numBus;
	CAN_Buffer_Write_Data(can_rx_buf[1]);
	if(conf.loger_run == true)
		CAN_Log_Buffer_Write_Data(can_rx_buf[1]);
	HAL_GPIO_WritePin(RX_LED_GPIO_Port, RX_LED_Pin, GPIO_PIN_SET);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	if(HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &can_rx_buf[0].header, can_rx_buf[0].data_byte) != HAL_OK) return;
	can_rx_buf[0].timestamp = HAL_GetTick();
	can_rx_buf[0].bus = eeprom_settings.numBus;
	CAN_Buffer_Write_Data(can_rx_buf[0]);
	if(conf.loger_run == true)
		CAN_Log_Buffer_Write_Data(can_rx_buf[0]);
	HAL_GPIO_WritePin(RX_LED_GPIO_Port, RX_LED_Pin, GPIO_PIN_SET);
}

void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef *hcan)
{
	HAL_CAN_RxFifo0MsgPendingCallback(hcan);
}

void HAL_CAN_RxFifo1FullCallback(CAN_HandleTypeDef *hcan)
{
	HAL_CAN_RxFifo1MsgPendingCallback(hcan);
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
	HAL_GPIO_WritePin(ERROR_LED_GPIO_Port, ERROR_LED_Pin, GPIO_PIN_SET);
	return;
}

HAL_StatusTypeDef Save_to_File(uint8_t * buf, uint32_t len)
{
	unsigned int bw;
	fresult = f_write(&fil, buf, len, &bw);
	if(fresult == FR_OK && bw == len) return HAL_OK;
	else return HAL_ERROR;
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
  MX_FATFS_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(100);


  EEPROM_Write_disable(&hspi2, 1000);


 // first initialization
  volatile boolean first = false;
  EEPROM_Read(&hspi2, EEPROM_SETINGS_ADDR, (uint8_t*)&eeprom_settings, sizeof(eeprom_settings));
  if(eeprom_settings.version == 0xFFFF || eeprom_settings.version < EEPROM_VERSION) {eeprom_settings.version = EEPROM_VERSION; first = true;}
  if(eeprom_settings.eeprom_size == 0xFFFF) {eeprom_settings.eeprom_size = EEPROM_SIZE; first = true;}
  if(eeprom_settings.number_of_busses == 0xFF) {eeprom_settings.number_of_busses = 4; first = true;}
  if(eeprom_settings.start_address_csript == 0xFFFF) {eeprom_settings.start_address_csript = EEPROM_SCRIPT_ADDR; first = true;}
  if(eeprom_settings.numBus == 0xFF) {eeprom_settings.numBus = 0; first = true;}
  if(eeprom_settings.CAN_Speed[0] == 0xFFFFFFFF) {eeprom_settings.CAN_Speed[0] = 500000; first = true;}
  if(eeprom_settings.CAN_Speed[1] == 0xFFFFFFFF) {eeprom_settings.CAN_Speed[1] = 125000; first = true;}
  if(eeprom_settings.CAN_Speed[2] == 0xFFFFFFFF) {eeprom_settings.CAN_Speed[2] = 33333; first = true;}
  if(eeprom_settings.CAN_Speed[3] == 0xFFFFFFFF) {eeprom_settings.CAN_Speed[3] = 10417; first = true;}
  for(int i=0; i<4;i++)
  {
	  if(eeprom_settings.CAN_mode[i] == 0xFFFFFFFF) {eeprom_settings.CAN_mode[i] = CAN_MODE_SILENT; first = true;}
  }
  if(eeprom_settings.UART_Speed == 0xFFFF) {eeprom_settings.UART_Speed = 115200; first = true;}
  if(eeprom_settings.fileOutputType > 10) {eeprom_settings.fileOutputType = CRTD_FILE; first = true;}
  if(eeprom_settings.SD_autoconnect > 10) {eeprom_settings.SD_autoconnect = 0; first = true;}

  if(first == true) EEPROM_Write(&hspi2, EEPROM_SETINGS_ADDR, (uint8_t*)&eeprom_settings, sizeof(eeprom_settings));



  // Check switch or incorrect data to reset UART speed
  if(HAL_GPIO_ReadPin(BOOT1_GPIO_Port, BOOT1_Pin) == 1)
  {
	  huart3.Init.BaudRate = 115200;
  }
  else
  {
	  huart3.Init.BaudRate = (eeprom_settings.UART_Speed > 0 && eeprom_settings.UART_Speed < 10000000) ? eeprom_settings.UART_Speed : 115200;
  }
  HAL_UART_Init(&huart3);

// SD FLASH try connect
  if(eeprom_settings.SD_autoconnect == 1)
  {
		fresult = f_mount(&fs, "0:", 1);
		if(fresult == FR_OK)
		{
			conf.sd_card_avalible = true;
			fresult = f_open(&fil, "script.txt", FA_READ);
			if(fresult == FR_OK)
			{
				do
				{
					if(f_gets((char*)script_read_buf, sizeof(script_read_buf), &fil) != 0)
					{
						uint8_t str_len = strlen((char*)script_read_buf);

						// cut of comments
						for(int i = 0; i < str_len; i++)
						{
							if(script_read_buf[i] == ' ' || script_read_buf[i] == '#' || script_read_buf[i] == '\r')
							{
								//if(i == 0) continue;
								script_read_buf[i] = '\r';
								str_len = i + 1;
								break;
							}
						}
						// correct "end of command"
						if(script_read_buf[str_len-1] == '\n') script_read_buf[str_len-1] = '\r';
						else if(script_read_buf[str_len-1] != '\r') script_read_buf[str_len++] = '\r';
						//if(debug_buf[str_len-2] == '\r') str_len--;

						if(str_len > 1)
							copy_script(script_read_buf, str_len);
					}
					else
					{
						script_read_buf[0] = 0xFF;
						copy_script(script_read_buf, 1);
						break;
					}
				}
				while(1);
			}
		}
		else
		{
			f_mount(0, "0:", 1);
			disk.is_initialized[0] = 0; // Reset flag of initialized
		}
  }


  conf.script_run = true;
  conf.scpipt_saving = false;
  conf.script_address = eeprom_settings.start_address_csript;
  conf.script_loop_address = 0xFFFF;




  SetFilterCAN(0,0,CAN_FILTERMODE_IDMASK,0);
  Change_CAN_channel();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
#if 0
  /* Enable the UART Parity Error Interrupt */
  __HAL_UART_ENABLE_IT(&huart3, UART_IT_PE);
#endif
  /* Enable the UART Error Interrupt: (Frame error, noise error, overrun error) */
  __HAL_UART_ENABLE_IT(&huart3, UART_IT_ERR);

  /* Enable the UART Data Register not empty Interrupt */
  __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);

  while (1)
  {


	  while(uart_rx_pointer_r != uart_rx_pointer_w) // Buffer UART RX reading
	  {
		  uart_rx_pointer_r = (uart_rx_pointer_r + 1) & 1023;
		  Check_Command(uart_rx_bufer[uart_rx_pointer_r]);

	  }

	  if(conf.script_run == true)
	  {
		  if(conf.script_delay_active && HAL_GetTick() - conf.script_timestamp < conf.script_delay)
		  {
			  ;
		  }
		  else
		  {
			  uint8_t in_byte;
			  conf.script_delay_active = false;
			  EEPROM_Read(&hspi2, conf.script_address, &in_byte, 1);
			  if(in_byte == CR)
			  {
				  exec_usb_cmd(script_buf);
				  for(;script_buf_pointer > 0; script_buf_pointer--)
				  {
					  script_buf[script_buf_pointer] = 0;
				  }
				  script_buf_pointer = 0;
				  if(conf.script_address < eeprom_settings.eeprom_size-1) conf.script_address++;
				  else conf.script_run = false;
			  }
			  else if(in_byte == 0xFF)
			  {
				  script_buf_pointer = 0;
				  if(conf.script_loop_address >= eeprom_settings.start_address_csript && conf.script_loop_address != 0xFFFF)
				  {
					  conf.script_address = conf.script_loop_address;
				  }
				  else
				  {
					  conf.script_run = false;
					  conf.script_address = eeprom_settings.start_address_csript;
					  script_buf_pointer = 0;
				  }
			  }
			  else
			  {
				  script_buf[script_buf_pointer] = in_byte;
				  script_buf_pointer++;
				  if(conf.script_address < (eeprom_settings.eeprom_size-1) && script_buf_pointer < CMD_BUFFER_LENGTH) conf.script_address++;
				  else conf.script_run = false;
			  }
		  }

	  }
	  else
	  {
		  conf.script_delay_active = false;
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
// LIN DEBUG
//			uint8_t temp_buf[9] = {0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55};
//			lin_send_master_request(0x55, temp_buf, 9);
//			HAL_GPIO_WritePin(TX_LED_GPIO_Port, TX_LED_Pin, GPIO_PIN_SET);
		}

		//Save file, open NEW
		if(conf.sd_card_avalible == true)
		{
			HAL_StatusTypeDef buff_stat;

			buff_stat = CAN_Log_Buffer_pull();

			if(HAL_GetTick() - time_stamp_SAVE >= LOG_FILE_SAVE_TIMEOUT
					|| ((conf.state == IDLE_ST || conf.loger_run == false) && buff_stat == HAL_BUSY)
					|| conf.fileOutputType != eeprom_settings.fileOutputType)
			{
				time_stamp_SAVE = HAL_GetTick();
				if(f_size(&fil) != 0 || conf.fileOutputType != eeprom_settings.fileOutputType) // TODO save new format check
				{
					fresult = f_close(&fil);
					if(conf.loger_run == true)
					{
						conf.fileOutputType = eeprom_settings.fileOutputType;
						  do
						  {
							  Generate_Next_FileName(filename, pathname);
							  fresult = f_open(&fil, (const TCHAR*)filename, FA_WRITE | FA_CREATE_NEW | FA_CREATE_ALWAYS);

							  if(fresult == FR_DENIED) // Full_directory
							  {
						        	do
						        	{
						        		Generate_Next_Path(pathname);
						        		fresult = f_mkdir((TCHAR*)pathname);
						        	}
						        	while(fresult == FR_EXIST);
						        	if(fresult != FR_OK) return ERROR;
						        	Generate_Next_FileName(filename, pathname);
						        	fresult = f_open(&fil, (const TCHAR*)filename, FA_WRITE | FA_CREATE_NEW | FA_CREATE_ALWAYS);
							  }
						  }
						  while(fresult == FR_EXIST);
					}
					if(fresult != FR_OK) conf.sd_card_avalible = false; //TODO flex. All errors disable card
				}
			}
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
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
  /** Initializes the CPU, AHB and APB buses clocks
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
