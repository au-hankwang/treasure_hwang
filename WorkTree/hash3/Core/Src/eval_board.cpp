#define STM32

#include <math.h>
#include <stdio.h>
#include <vector>
#include "main.h"
#include "usb_device.h"
#include "../../../miner/helper.h"
#include "eval_board.h"
#include "low_level.h"
#include "ssd1306.h"


extern uc_serialtype serial1, serial2, serial3;
extern short scv_millivolts, io_millivolts, core_millivolts;
extern short fan_pwm_setting;


bool pc_seen = false;
bool aaps_present = false;
char oled_raw_string[64] ;
char display0_string[32], display1_string[32], display2_string[32];
boardinfotype boardinfo;
int partial_bytes=0;
uint8_t commandbuf[MAX_PACKET_SIZE];
uint8_t responsebuf[MAX_PACKET_SIZE];
char errorString[56];


extern "C" void DebugBreakFunc(const char* module, int line)
   {
   sprintf(errorString, "Fatal_error: %s %d", module, line);
   DebugString(errorString);
   }

extern "C" void Usb_Receive(void * buf, int len)
   {
   usbcommandtype& uc = *(usbcommandtype*)commandbuf;
   usbresponsetype& ur = *(usbresponsetype*)responsebuf;
   char* command_char = (char*)commandbuf + sizeof(usbcommandtype);
   unsigned char* command_uchar = (unsigned char*)commandbuf + sizeof(usbcommandtype);
   int* command_word = (int*)((char*)commandbuf + sizeof(usbcommandtype));
   char* response_char = (char*)responsebuf + sizeof(usbresponsetype);
   unsigned char* response_uchar = (unsigned char*)responsebuf + sizeof(usbresponsetype);
   int* response_word = (int*)((char*)responsebuf + sizeof(usbresponsetype));

   LED_Blue(true);
   if (partial_bytes) {
      if (partial_bytes + len > MAX_PACKET_SIZE) {
         sprintf(response_char, "partial_bytes=%d + len=%d too large\n", partial_bytes,len);
         partial_bytes = 0;
         DebugString(response_char);
         ur.command = uc.command;
         ur.unique = 0x321e;
         ur.length = strlen(response_char);
         strncpy(errorString, response_char, MINIMUM(ur.length, sizeof(errorString)));
         CDC_Transmit_FS(responsebuf, sizeof(usbresponsetype) + ur.length);
         return;
         }
      }
   if (len >= MAX_PACKET_SIZE) {
      sprintf(response_char, "len=%d too large\n", len);
      partial_bytes = 0;
      DebugString(response_char);
      ur.command = uc.command;
      ur.unique = 0x321e;
      ur.length = strlen(response_char);
      strncpy(errorString, response_char, MINIMUM(ur.length, sizeof(errorString)));
      CDC_Transmit_FS(responsebuf, sizeof(usbresponsetype) + ur.length);
      return;
      }
   memcpy(commandbuf + partial_bytes, buf, len);
   partial_bytes += len;

   ur.command = uc.command;
   ur.extra = 0;
   ur.length = 0;

   if (partial_bytes < (int)sizeof(usbcommandtype))
      {
      sprintf(response_char, "I received a usb packet too small, len=%d\n", len);
      partial_bytes = 0;
      DebugString(response_char);
      ur.length = strlen(response_char);
      strncpy(errorString, response_char, MINIMUM(ur.length, sizeof(errorString)));
      CDC_Transmit_FS(responsebuf, sizeof(usbresponsetype) + ur.length);
      return;
      }
   if (uc.unique!=0x8dac)
      {
      sprintf(response_char, "I received a usb packet with a bad unique=%x\n", uc.unique);
      partial_bytes = 0;
      DebugString(response_char);
      ur.length = strlen(response_char);
      strncpy(errorString, response_char, MINIMUM(ur.length, sizeof(errorString)));
      CDC_Transmit_FS(responsebuf, sizeof(usbresponsetype) + ur.length);
      return;
      }
   if (partial_bytes < (int)sizeof(usbcommandtype) + uc.length)
      {
      return;
      }
   switch (uc.command) {
      case UI_NOTHING: break;
      case UI_VERSION: 
#ifdef HASH3_PROJECT
         sprintf(response_char, "Hash 3 boards, %s %s\n", __DATE__, __TIME__);
#endif
#ifdef EVAL_BOARD_PROJECT
         sprintf(response_char, "Eval 1 asic, %s %s\n", __DATE__, __TIME__);
#endif
#ifdef TREASURE_EVAL_BOARD_PROJECT
         sprintf(response_char, "Eval 1 asic, %s %s\n", __DATE__, __TIME__);
#endif
#ifdef EVAL_3ASIC_BOARD_PROJECT
         sprintf(response_char, "Eval 3 asic, %s %s\n", __DATE__, __TIME__);
#endif
         ur.length = strlen(response_char);
         pc_seen = true;
         break;
      case UI_ERRORSTRING: 
         memcpy(response_char, errorString, sizeof(errorString)); 
         ur.length = errorString[0] ? sizeof(errorString) : 0; 
         memset(errorString, 0, sizeof(errorString));
         break;
      case UI_GPIO_READWRITE:
         gpiouniontype gu;
         gu.s = uc.extra;
         SetGPIO(gu.gpio);
         ur.extra = ReadGPIO();
         break;
      case UI_DEBUG:
         ur.length = 8;
         response_word[0] = serial1.tail;
         response_word[1] = sizeof(serial1.rxbuf) - serial1.huart.hdmarx->Instance->CNDTR;
         break;
      case UI_SETBAUD:
         if (uc.length != 4)
            sprintf(errorString, "Length field for UI_BAUD is not 4, len=%d",uc.length);
         serial1.Init(command_word[0]);
         break;
      case UI_SERIAL:
         serial1.Push(uc.length, command_uchar);
         serial1.Pop(ur.length, response_uchar);
         break;
      case UI_SUPPLY:
         if (uc.extra == 0) { // Core
            CORE_Supply(command_word[0]);
            if (uc.length != 4)
               strcpy(errorString, "Length!=4 for UI_SUPPLY, CORE");
            }
         else if (uc.extra == 1) { // SCV
            SCV_Supply(command_word[0]);
            if (uc.length!=4)
               strcpy(errorString, "Length!=4 for UI_SUPPLY, SCV");
            }
         else if (uc.extra == 2) { // IO
            IO_Supply(command_word[0]);
            if (uc.length != 4)
               strcpy(errorString, "Length!=4 for UI_SUPPLY, IO");
            }
         break;
      case UI_FAN:
         PWM_fan(uc.extra);
         break;
      case UI_BOARD:
#ifdef rx_enable1_GPIO_Port
         HAL_GPIO_WritePin(rx_enable1_GPIO_Port, rx_enable1_Pin, (uc.extra & 0x001) ? GPIO_PIN_SET : GPIO_PIN_RESET);
         HAL_GPIO_WritePin(tx_enable1_GPIO_Port, tx_enable1_Pin, (uc.extra & 0x100) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef rx_enable2_GPIO_Port
         HAL_GPIO_WritePin(rx_enable2_GPIO_Port, rx_enable2_Pin, (uc.extra & 0x002) ? GPIO_PIN_SET : GPIO_PIN_RESET);
         HAL_GPIO_WritePin(tx_enable2_GPIO_Port, tx_enable2_Pin, (uc.extra & 0x200) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef rx_enable3_GPIO_Port
         HAL_GPIO_WritePin(rx_enable3_GPIO_Port, rx_enable3_Pin, (uc.extra & 0x004) ? GPIO_PIN_SET : GPIO_PIN_RESET);
         HAL_GPIO_WritePin(tx_enable3_GPIO_Port, tx_enable3_Pin, (uc.extra & 0x400) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
         break;
      case UI_ENABLE:
#ifdef vdd_enable1_GPIO_Port
         HAL_GPIO_WritePin(vdd_enable1_GPIO_Port, vdd_enable1_Pin, (uc.extra & 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef vdd_enable2_GPIO_Port
         HAL_GPIO_WritePin(vdd_enable2_GPIO_Port, vdd_enable2_Pin, (uc.extra & 2) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef vdd_enable3_GPIO_Port
         HAL_GPIO_WritePin(vdd_enable3_GPIO_Port, vdd_enable3_Pin, (uc.extra & 4) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef thermal_trip1_GPIO_Port
         HAL_GPIO_WritePin(thermal_trip1_GPIO_Port, thermal_trip1_Pin, (uc.extra & 0x10) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef thermal_trip2_GPIO_Port
         HAL_GPIO_WritePin(thermal_trip2_GPIO_Port, thermal_trip2_Pin, (uc.extra & 0x20) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef thermal_trip3_GPIO_Port
         HAL_GPIO_WritePin(thermal_trip3_GPIO_Port, thermal_trip3_Pin, (uc.extra & 0x40) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
         break;
      case UI_ANALOG:
         Adc_ChangeChannel(!!(uc.extra&1));
         Dac2(uc.extra >> 4);
         break;
      case UI_ZERO_ADC:
         Adc_Zero();
         break;
      case UI_SET_OVERCURRENT_LIMIT:
         PmbusWrite16(0x46, uc.extra*10);   
         PmbusWrite8(0x47, 0xc4);   // enable oc fault
         break;
      case UI_BOARDINFO:
         ur.length = sizeof(boardinfo);
         memcpy(response_uchar, &boardinfo, sizeof(boardinfo));
         break;
      case UI_I2C_DONE:
         ur.extra = I2C_Done() ? 1:0;
         break;
      case UI_OLED: 
         memcpy(oled_raw_string, command_char, MINIMUM(uc.length, sizeof(oled_raw_string)));
         oled_raw_string[MINIMUM(uc.length, sizeof(oled_raw_string)-1)] = 0;
         break;
      case UI_ENABLE_DISABLE_IMMERSION:
         PmbusWrite8(0xe7, uc.extra);
         break;
      }
   ur.unique = errorString[0] ? 0x321e : 0x321f;
   partial_bytes = 0;
   CDC_Transmit_FS(responsebuf, sizeof(usbresponsetype) + ur.length);
   LED_Blue(false);
   }



extern "C" void Init()
   {
   LED_Blue(true);
   strcpy(display0_string, "Uctl up");
   aaps_present = false;

#ifdef HASH3_PROJECT
   serial3.Init(115200);
   boardinfo.boards_present = 0;
   I2C_Switch(0);
   boardinfo.boards_present |= I2cPresent_Display(0x4f) ? 1 : 0;
   I2C_Switch(1);
   boardinfo.boards_present |= I2cPresent_Display(0x4f) ? 2 : 0;
   I2C_Switch(2);
   boardinfo.boards_present |= I2cPresent_Display(0x4f) ? 4 : 0;
   PWM_fan(100);
#endif

   I2cScan();
   SSD1306_Init();
   OLED_Display(display0_string, display1_string, errorString[0] ? errorString : display2_string);
   SSD1306_UpdateScreen();
#ifdef HASH3_PROJECT
   serial3.Init(115200);
   I2C_Switch(1);
//   I2cPresent(0x10);
//   I2cScan();
#else
   SCV_Supply(800);
   IO_Supply(1200);
   CORE_Supply(300);
#endif

#ifdef EVAL_BOARD_PROJECT
   SCV_Supply(0);
   IO_Supply(0);
   CORE_Supply(0);
   Adc_Init();
   ISL_Init();
   for (int i=0; i<16; i++) I2C_Work();
   strcpy(display0_string, "ADC cal");
   OLED_Display(display0_string, display1_string, errorString[0] ? errorString : display2_string);
   SSD1306_UpdateScreen();
   HAL_Delay(1000);
   boardinfo.implied_reference = 3.24;
/*
//   Adc_Zero();
   I2C_Work();
   SCV_Supply(800);
   IO_Supply(1200);
   CORE_Supply(0);
   for (int i = 0; i < 16; i++) I2C_Work();
   HAL_Delay(500);
   Adc_ChangeChannel(true);
   HAL_Delay(500);
   for (int i = 0; i < 16; i++) I2C_Work();
   float zero_value = boardinfo.raw_adc[2];
   CORE_Supply(380); // make sure this value *4 < 3.3V/2
   for (int i = 0; i < 16; i++) I2C_Work();
   HAL_Delay(500);
   for (int i = 0; i < 16; i++) I2C_Work();
   float final_value = boardinfo.raw_adc[2];
   boardinfo.implied_reference = (4.0 * .380) / ((final_value-zero_value)/65536.0);
   Adc_ChangeChannel(false);
*/   
   HAL_GPIO_WritePin(enable_25mhz_GPIO_Port, enable_25mhz_Pin, GPIO_PIN_SET);
#ifdef enable_100mhz_GPIO_Port
   HAL_GPIO_WritePin(enable_100mhz_GPIO_Port, enable_100mhz_Pin, GPIO_PIN_RESET);
#endif
   HAL_GPIO_WritePin(response_i_GPIO_Port, response_i_Pin, GPIO_PIN_RESET);
   sprintf(oled_raw_string, "Waiting for PC\nImplied Ref=%.3fV", boardinfo.implied_reference);
   Dac2(0);
   CORE_Supply(0);
#endif

#ifdef EVAL_3ASIC_BOARD_PROJECT
   CORE_Supply(0);
   Adc_Init();
   if (I2cPresent_Pmbus(0x60))
      {
      ISL_Init();
      strcpy(display1_string, "ISL Present");
      }
   else
      strcpy(display1_string, "ISL Absent");
   boardinfo.implied_reference = 3.225;
   for (int i = 0; i < 16; i++) I2C_Work();
   OLED_Display(display0_string, display1_string, errorString[0] ? errorString : display2_string);
   SSD1306_UpdateScreen();
   for (int i = 0; i < 16; i++) I2C_Work();
   HAL_Delay(1000);
   CORE_Supply(1000);
   SCV_Supply(800);
   IO_Supply(1200);
   sprintf(oled_raw_string, "Waiting for SW\n#\n@");
   HAL_GPIO_WritePin(enable_25mhz_GPIO_Port, enable_25mhz_Pin, GPIO_PIN_SET);
   HAL_GPIO_WritePin(enable_100mhz_GPIO_Port, enable_100mhz_Pin, GPIO_PIN_RESET);
   HAL_GPIO_WritePin(response_i_GPIO_Port, response_i_Pin, GPIO_PIN_RESET);
   for (int i = 0; i < 16; i++) I2C_Work();
#endif
#ifdef HASH3_PROJECT
   CORE_Supply(0);
   if (I2cPresent_Pmbus(0x10))
      {
      Boco_Init();
      strcpy(display1_string, "Boco Present");
      }
   else if (I2cPresent_Pmbus(0x58))
      {
      aaps_present = true;
      Aaps_Init();
      strcpy(display1_string, "AAPS Present");
      }
   else
      strcpy(display1_string, "ISL Absent");
   boardinfo.implied_reference = 3.225;
   for (int i = 0; i < 16; i++) I2C_Work();
   OLED_Display(display0_string, display1_string, errorString[0] ? errorString : display2_string);
   SSD1306_UpdateScreen();
   for (int i = 0; i < 16; i++) I2C_Work();
   HAL_Delay(1000);
   sprintf(oled_raw_string, "System, ready\n%d Boards [%c%c%c]\n#", ((boardinfo.boards_present>>0)&1)+ ((boardinfo.boards_present >> 1) & 1)+ ((boardinfo.boards_present >> 2) & 1), ((boardinfo.boards_present >> 0) & 1)?'0':' ', ((boardinfo.boards_present >> 1) & 1) ? '1' : ' ', ((boardinfo.boards_present >> 2) & 1) ? '2' : ' ');
   for (int i = 0; i < 16; i++) I2C_Work();
#endif

   LED_Blue(false);
   }
extern "C" void Loop()
   {
   unsigned int tick = HAL_GetTick();

   I2C_Work();

   bool halfsecond = (tick >> 9) & 1;
   LED_Red(halfsecond);

   char* chptr, * begin, working_string[64], insertstring[64];
   strcpy(working_string, oled_raw_string);
   begin = working_string;
   if ((chptr = strchr(begin, '@')) != NULL) {
      sprintf(insertstring, "%d, %d, %d mV%s", io_millivolts, scv_millivolts, core_millivolts, chptr + 1);
      strcpy(chptr, insertstring);
      }
   if ((chptr = strchr(begin, '$')) != NULL) {
      sprintf(insertstring, "%.2fW%s", core_millivolts * boardinfo.core_current, chptr + 1);
      strcpy(chptr, insertstring);
      }
#ifdef HASH3_PROJECT
   if ((chptr = strchr(begin, '#')) != NULL) {
      sprintf(insertstring, "%4.1fV, %4.1fW %4.1fA %s", boardinfo.supply_voltage, boardinfo.supply_pin, boardinfo.supply_current, chptr + 1);
      strcpy(chptr, insertstring);
      }
   if ((chptr = strchr(begin, '&')) != NULL) {
      sprintf(insertstring, "Fan %4.1f%% %s", fan_pwm_setting / 2.55, chptr + 1);
      strcpy(chptr, insertstring);
      }

   // thermal trip is active low(0=bad), turn off the supply if it asserts
   if (false && HAL_GPIO_ReadPin(reset_n_GPIO_Port, reset_n_Pin) == GPIO_PIN_SET) {
      if (HAL_GPIO_ReadPin(thermal_trip1_GPIO_Port, thermal_trip1_Pin) != GPIO_PIN_SET)
         {
         CORE_Supply(0);
         strcpy(errorString, "ThermalTrip #0");
         }
      else if (HAL_GPIO_ReadPin(thermal_trip2_GPIO_Port, thermal_trip2_Pin) != GPIO_PIN_SET)
         {
         CORE_Supply(0);
         strcpy(errorString, "ThermalTrip #1");
         }
      else if (HAL_GPIO_ReadPin(thermal_trip3_GPIO_Port, thermal_trip3_Pin) != GPIO_PIN_SET)
         {
         CORE_Supply(0);
         strcpy(errorString, "ThermalTrip #2");
         }
   }

#else
   if ((chptr = strchr(begin, '#')) != NULL) {
      sprintf(insertstring, "%4.1f, %4.1f, %4.1fA %s", boardinfo.io_current * 1000.0, boardinfo.scv_current, boardinfo.core_current, chptr + 1);
      strcpy(chptr, insertstring);
      }
#endif
   chptr = strchr(begin, '\n');
   if (chptr == NULL)
      {
      strcpy(display0_string, begin);
      display1_string[0] = 0;
      display2_string[0] = 0;
      }
   else {
      *chptr = 0;
      strcpy(display0_string, begin);
      begin = chptr + 1;
      chptr = strchr(begin, '\n');
      if (chptr == NULL) {
         strcpy(display1_string, begin);
         display2_string[0] = 0;
         }
      else {
         *chptr = 0;
         strcpy(display1_string, begin);
         strcpy(display2_string, chptr+1);
         }
      }
   if (boardinfo.faults != 0)
      sprintf(errorString, "ISL FAULT=%X", boardinfo.faults);
   if (errorString[0])
      strcpy(display2_string, errorString);

   }
