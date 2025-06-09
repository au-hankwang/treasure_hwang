/*
 * low_level.h
 *
 *  Created on: Feb 5, 2023
 *      Author: dcarlson
 */

#ifndef INC_LOW_LEVEL_H_
#define INC_LOW_LEVEL_H_


const int serial_fifo_size = 1024;
class uc_serialtype {
public:
   const int port;
   UART_HandleTypeDef &huart;
   int txbytes_yet_to_send, tail;
   uint8_t txbuf[serial_fifo_size], rxbuf[serial_fifo_size];
public:
   void Push(const unsigned short& length, const uint8_t* command_char);
   void Pop(unsigned short& length, uint8_t* response_char);
   void Init(int baud);
   uc_serialtype(UART_HandleTypeDef &_huart, int _port) : port(_port), huart(_huart)  {
      txbytes_yet_to_send = 0;
      tail = 0;
      }
   };

void I2C_Work();
void ISL_Init();
void Boco_Init();
void Aaps_Init();
void Adc_Init();
void Adc_ChangeChannel(bool single_ended);
void Adc_Zero();
void Dac2(int value); // 12b analog value for dac2
void LED_Blue(bool enable);
void LED_Red(bool enable);
void SetGPIO(const gpiotype& gpio);
unsigned short ReadGPIO();
void SCV_Supply(int millivolts);
void IO_Supply(int millivolts);
void CORE_Supply(int millivolts);
void PWM_fan(int value);
void I2C_Switch(int board);   // board=0,1,2
void I2cScan();
bool I2cPresent_Pmbus(int address);
bool I2cPresent_Display(int address);
bool I2C_Done();
void PmbusWrite0(short command);
void PmbusWrite8(short command, unsigned char data);
void PmbusWrite16(short command, unsigned short data);

extern "C" void DebugString(const char* txt);
extern "C" void Loop();
extern "C" void Init();
extern "C" void Usb_Receive(void* Buf, int len);
extern "C" uint8_t CDC_Transmit_FS(uint8_t * Buf, uint16_t Len);
void DebugBreakFunc(const char* module, int line);
void OLED_Display(char* string1, char* string2, char* string3);
void SSD1306_UpdatePartialScreen(int row);


#endif /* INC_LOW_LEVEL_H_ */
