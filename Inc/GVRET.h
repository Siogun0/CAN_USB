/*
 * GEVCU.h
 *
Copyright (c) 2013 Collin Kidder, Michael Neuweiler, Charles Galpin

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#ifndef GVRET_H_
#define GVRET_H_
#include <stdint.h>
#include "main.h"


#define CFG_BUILD_NUM	343
#define CFG_VERSION "GVRET alpha 2017-11-09"
#define EEPROM_PAGE		275 //this is within an eeprom space currently unused on GEVCU so it's safe
#define EEPROM_VER		0x17

typedef enum
{
	false = 0,
	true = 1
}boolean;


/*#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC
#define HS_CAN_EN_Pin GPIO_PIN_4
#define HS_CAN_EN_GPIO_Port GPIOA
#define FT_CAN_EN_Pin GPIO_PIN_5
#define FT_CAN_EN_GPIO_Port GPIOA
#define SW_CAN_EN_Pin GPIO_PIN_6
#define SW_CAN_EN_GPIO_Port GPIOA
#define LIN_EN_Pin GPIO_PIN_7
#define LIN_EN_GPIO_Port GPIOA
#define MODE_SEL_Pin GPIO_PIN_0
#define MODE_SEL_GPIO_Port GPIOB
#define BUTTON_Pin GPIO_PIN_1
#define BUTTON_GPIO_Port GPIOB
#define BOOT1_Pin GPIO_PIN_2
#define BOOT1_GPIO_Port GPIOB
#define LIN_TX_Pin GPIO_PIN_10
#define LIN_TX_GPIO_Port GPIOB
#define LIN_RX_Pin GPIO_PIN_11
#define LIN_RX_GPIO_Port GPIOB*/
/* USER CODE BEGIN Private defines */
#define HW_VER        0x10		// hardware version
#define SW_VER        0x10		// software version
#define SW_VER_MAJOR  0x01    // software major version
#define SW_VER_MINOR  0x0A    // software minor version
#define SERIAL        "0001"	// device serial number

#if !defined(CR)
#define CR            13	// command end tag (ASCII CR)
#endif

#if !defined(ERROR)
#define ERROR         7		// error tag (ASCII BEL)
#endif

#define SET_BITRATE     	'S'	// set CAN bit rate
#define SET_CUSTOM_BTR  	's'	// set CAN bit rate via
#define OPEN_CAN_CHAN   	'O'	// open CAN channel
#define CLOSE_CAN_CHAN  	'C'	// close CAN channel
#define SEND_11BIT_ID   	't'	// send CAN message with 11bit ID
#define SEND_29BIT_ID   	'T'	// send CAN message with 29bit ID
#define SEND_R11BIT_ID  	'r'	// send CAN remote message with 11bit ID
#define SEND_R29BIT_ID  	'R'	// send CAN remote message with 29bit ID
#define READ_STATUS     	'F'	// read status flag byte
#define SET_FILTER_ID      	'M'	// set Acceptance Code Register
#define SET_FILTER_MASK    	'm'	// set Acceptance Mask Register
#define SET_FILTER			'f' // set filter number (0-13) and type (M - mask, I - ID) 'f1I'
#define GET_VERSION     	'V'	// get hardware and software version
#define GET_SW_VERSION 	 	'v' // get software version only
#define GET_SERIAL    	  	'N'	// get device serial number
#define TIME_STAMP    	  	'Z'	// toggle time stamp setting
#define READ_ECR     	   	'E'	// read Error Capture Register
#define READ_ALCR    	   	'A'	// read Arbritation Lost Capture Register
#define READ_REG_COM    	'G'	// read register conten from SJA1000
#define WRITE_REG_COM   	'W'	// write register content to SJA1000
#define LISTEN_ONLY     	'L'	// switch to listen only mode
#define SET_USART_BTR_CUST	'u' // set USART bitrate
#define SET_USART_BTR		'U' // set USART bitrate
#define SELECT_BUS_CHANEL	'B' // select channel CAN/LIN
#define PROGRAM_SCRIPT		'P' // start = 1/stop = 0/print = 2 saving commands to memory
#define JUMP_MARKER			'J' // marker for jump script after end (loop start)
#define DELAY_MS			'D' // delay before next script command
#define STOP_SCRIPT			'x' // stop script
#define START_SCRIPT		'X' // start script
#define START_LOGGING		'H' // start logging
#define STOP_LOGGING		'h' // stop logging
#define HELP				'?' // list of supported commands
#define RESET				'!' // !RST - reset

#define TIME_STAMP_TICK 1000	// microseconds
#define CMD_BUFFER_LENGTH  30

// calculate timer0 overflow value

#define OCR_VALUE ((unsigned char)((unsigned long)(TIME_STAMP_TICK) / (1000000L / (float)((unsigned long)F_CPU / 64L))))


typedef enum  {
    IDLE = 0,
    GET_COMMAND = 1,
    BUILD_CAN_FRAME = 2,
    TIME_SYNC = 3,
    GET_DIG_INPUTS = 4,
    GET_ANALOG_INPUTS = 5,
    SET_DIG_OUTPUTS = 6,
    SETUP_CANBUS = 7,
    GET_CANBUS_PARAMS = 8,
    GET_DEVICE_INFO = 9,
    SET_SINGLEWIRE_MODE = 10,
    SET_SYSTYPE = 11,
    ECHO_CAN_FRAME = 12,
    SETUP_EXT_BUSES = 13
} STATE;

typedef enum
{
    PROTO_BUILD_CAN_FRAME = 0,
    PROTO_TIME_SYNC = 1,
    PROTO_DIG_INPUTS = 2,
    PROTO_ANA_INPUTS = 3,
    PROTO_SET_DIG_OUT = 4,
    PROTO_SETUP_CANBUS = 5,
    PROTO_GET_CANBUS_PARAMS = 6,
    PROTO_GET_DEV_INFO = 7,
    PROTO_SET_SW_MODE = 8,
    PROTO_KEEPALIVE = 9,
    PROTO_SET_SYSTYPE = 10, //A
    PROTO_ECHO_CAN_FRAME = 11, //B
    PROTO_GET_NUMBUSES = 12, //C
    PROTO_GET_EXT_BUSES = 13, //D
    PROTO_SET_EXT_BUSES = 14 //E
} GVRET_PROTOCOL;

typedef struct {
    uint8_t eepromWPPin;
    uint8_t CAN0EnablePin;
    uint8_t CAN1EnablePin;
    uint8_t SWCANMode0Pin;
    uint8_t SWCANMode1Pin;
    boolean useSD; //should we attempt to use the SDCard? (No logging possible otherwise)
    boolean logToFile; //are we currently supposed to be logging to file?
    boolean dedicatedSWCAN; //true if there is a dedicated SWCAN channel. Found on CANDue 2.2 or higher boards
    uint8_t SDCardSelPin;
    boolean SDCardInserted;
    uint8_t LED_CANTX;
    uint8_t LED_CANRX;
    uint8_t LED_LOGGING;
    boolean txToggle; //LED toggle values
    boolean rxToggle;
    boolean logToggle;
    boolean lawicelMode;
    boolean lawicelAutoPoll;
    boolean lawicelTimestamping;
    int lawicelPollCounter;
    int8_t numBus;
    uint32_t CAN_Speed[4];
    uint32_t CAN_mode[4];
} SystemSettings;

typedef enum {
    NONE = 0,
    BINARYFILE = 1,
    GVRET_FILE = 2,
    CRTD_FILE = 3
} FILEOUTPUTTYPE;

volatile typedef struct {
	uint16_t version;
	uint16_t eeprom_size;
	uint8_t number_of_busses;
	uint8_t reserv1;
	uint16_t start_address_csript;

	uint8_t numBus;
    uint32_t CAN_Speed[4];
    uint32_t reserv2[6];
    uint32_t CAN_mode[4];
    uint32_t reserv3[6];

    uint32_t UART_Speed;
    uint32_t reserv4;
    FILEOUTPUTTYPE fileOutputType; //what format should we use for file output?
} t_eeprom_settings;

typedef struct {  //should be 10 bytes
    uint32_t id;
    uint32_t mask;
    boolean extended;
    boolean enabled;
} FILTER;



typedef struct { //Must stay under 256 - currently somewhere around 222
    uint8_t version;

    uint32_t CAN0Speed;
    uint32_t CAN1Speed;
    uint32_t SWCANSpeed;
    boolean CAN0_Enabled;
    boolean CAN1_Enabled;
    boolean singleWire_Enabled; //On older hardware tries to turn CAN1 into SW, newer hardware has dedicated chips for it
    FILTER CAN0Filters[8]; // filters for our 8 mailboxes - 10*8 = 80 bytes
    FILTER CAN1Filters[8]; // filters for our 8 mailboxes - 10*8 = 80 bytes

    boolean useBinarySerialComm; //use a binary protocol on the serial link or human readable format?
    FILEOUTPUTTYPE fileOutputType; //what format should we use for file output?

    char fileNameBase[30]; //Base filename to use
    char fileNameExt[4]; //extension to use
    uint16_t fileNum; //incrementing value to append to filename if we create a new file each time
    boolean appendFile; //start a new file every power up or append to current?
    boolean autoStartLogging; //should logging start immediately on start up?

    uint8_t logLevel; //Level of logging to output on serial line
    uint8_t sysType; //0 = CANDUE, 1 = GEVCU, 2 = CANDue 1.3 to 2.1, 3 = CANDue 2.2

    uint16_t valid; //stores a validity token to make sure EEPROM is not corrupt

    boolean CAN0ListenOnly; //if true we don't allow any messing with the bus but rather just passively monitor.
    boolean CAN1ListenOnly;
    boolean SWCANListenOnly;
} EEPROMSettings;

typedef enum
{
	DIR_RECEIVE,
	DIR_TRANSMIT
} can_dir_t;

typedef struct
{
//	enum {STANDARD, EXTENDED} format;
//	uint32_t id;
//	enum {REMOTE, DATA} type;
//	uint8_t length;
	uint32_t timestamp;
	CAN_RxHeaderTypeDef header;
	union
	{
		uint8_t data_byte[8];
		uint32_t data_word[2];
		uint64_t data_full;
	};
	can_dir_t can_dir;
} can_msg_t;


typedef struct
{
	enum {IDLE_ST, LAWICEL_CONNECT, SAVVYCAN_CONNECT} state;
	boolean timestamp_en;
	boolean useBinarySerialComm;
	boolean CAN_Enable[4];
	boolean scpipt_saving;
	boolean script_run;
	boolean script_print;
	uint16_t script_address;
	uint16_t script_loop_address;
	uint32_t script_timestamp;
	uint32_t script_delay;
	boolean script_delay_active;
	boolean help_print;
	uint16_t help_text_pointer;
	uint32_t filter_id;
	uint32_t filter_mask;
	uint32_t filter_num;
	uint32_t filter_mode;
} conf_t;

extern t_eeprom_settings eeprom_settings;

void loadSettings();
void setSWCANSleep();
void setSWCANEnabled();
void setSWCANWakeup();
//void processDigToggleFrame(CAN_FRAME &frame);
void sendDigToggleMsg();
void Check_Command(uint8_t in_byte);
uint8_t HexToShort (uint8_t H, uint8_t L);
uint8_t HexTo4bits (uint8_t H);
void ShortToHex (uint8_t in, uint8_t * out);
uint16_t BuildFrameToUSB (can_msg_t frame, int whichBus, uint8_t * buf);
HAL_StatusTypeDef CAN_Buffer_pull(void);
HAL_StatusTypeDef CAN_Log_Buffer_pull(void);
HAL_StatusTypeDef CAN_Buffer_Write_Data(can_msg_t msg);
HAL_StatusTypeDef CAN_Log_Buffer_Write_Data(can_msg_t msg);
void CAN_Buffer_Init(void);
void CAN_Buffer_clean(void);
HAL_StatusTypeDef Open_CAN_cannel(void);
HAL_StatusTypeDef Close_CAN_cannel(void);
void Change_CAN_channel(void);
void Next_CAN_channel (void);
HAL_StatusTypeDef Close_LIN_cannel(void);
HAL_StatusTypeDef Open_LIN_cannel(void);
void STM_bxCAN_calc(uint32_t freq, float bitrate, CAN_HandleTypeDef * hcan);
uint8_t exec_usb_cmd (uint8_t * cmd_buf);
HAL_StatusTypeDef SetFilterCAN(uint32_t id, uint32_t mask_or_id, uint32_t mode, uint32_t num);
void Generate_Next_FileName(uint8_t * name);
uint16_t BuildFrameToFile(can_msg_t frame, int whichBus, uint8_t * buff);
#endif /* GVRET_H_ */



