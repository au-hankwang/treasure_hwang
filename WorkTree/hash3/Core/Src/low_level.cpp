#define STM32

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include "main.h"
#include "eval_board.h"
#include "low_level.h"


extern char display0_string[20], display1_string[20], display2_string[20];

short adc1[256], adc2[256], adc3[256];
bool single_ended;
float pin_median[17];

short scv_value, io_value, core_value;
short scv_millivolts, io_millivolts, core_millivolts;
short fan_pwm_setting;
bool i2c_busy, isl_is_configured;
extern boardinfotype boardinfo;
extern UART_HandleTypeDef huart1;
uc_serialtype serial1(huart1, 1);
unsigned char psu_address = 0x60;
signed char psu_exponent = 0;
extern bool aaps_present;


float linear16(unsigned char high, unsigned char low)
   {
   short mantissa = (high << 8) | low;
   float ret = mantissa;
   int i;
   for (i = 0; i < psu_exponent; i++)
      ret *= 2.0;
   for (i = 0; i > psu_exponent; i--)
      ret *= 0.5;
   return ret;
   }

float linear11(unsigned char high, unsigned char low)
   {
   signed char exponent = (signed char)high >> 3;
   short mantissa = ((high & 0x7) << 8) + low;
   if (exponent&0x10) exponent |= 0xf0;
   if (mantissa & 0x400) mantissa |= 0xfc;
   float ret = mantissa;
   int i;
   for (i = 0; i < exponent; i++)
      ret *= 2.0;
   for (i = 0; i > exponent; i--)
      ret *= 0.5;
   return ret;
   }

#ifdef HASH3_PROJECT
  extern TIM_HandleTypeDef htim19;
  extern UART_HandleTypeDef huart2;
  extern UART_HandleTypeDef huart3;
  uc_serialtype serial2(huart2, 2), serial3(huart3, 3);
  extern I2C_HandleTypeDef hi2c1;
  extern I2C_HandleTypeDef hi2c2;
  I2C_HandleTypeDef& hi2c_display = hi2c1;
  I2C_HandleTypeDef& hi2c_pmbus = hi2c2;
//  unsigned char psu_address = 0x10; // boco address
//  unsigned char psu_address = 0x58; // aaps address
#endif

#ifdef EVAL_BOARD_PROJECT
  extern DAC_HandleTypeDef hdac2;
  UART_HandleTypeDef huart2, huart3;
  uc_serialtype serial2(huart2, 2), serial3(huart3, 3);

  extern I2C_HandleTypeDef hi2c1;
  I2C_HandleTypeDef& hi2c_temp = hi2c1;
  I2C_HandleTypeDef& hi2c_dpot = hi2c1;
  I2C_HandleTypeDef& hi2c_display = hi2c1;
  I2C_HandleTypeDef& hi2c_pmbus = hi2c1;

  extern SDADC_HandleTypeDef hsdadc1;
  extern SDADC_HandleTypeDef hsdadc2;
  extern SDADC_HandleTypeDef hsdadc3;
#endif

#ifdef EVAL_3ASIC_BOARD_PROJECT
  UART_HandleTypeDef huart2, huart3;
  uc_serialtype serial2(huart2, 2), serial3(huart3, 3);

  extern I2C_HandleTypeDef hi2c1;
  I2C_HandleTypeDef& hi2c_temp = hi2c1;
  I2C_HandleTypeDef& hi2c_dpot = hi2c1;
  I2C_HandleTypeDef& hi2c_display = hi2c1;
  I2C_HandleTypeDef& hi2c_pmbus = hi2c1;

  extern SDADC_HandleTypeDef hsdadc1;
#endif

bool I2cPresent_Pmbus(int address) {
   uint8_t ret = HAL_I2C_IsDeviceReady(&hi2c_pmbus, (uint16_t)(address << 1), 3, 5);
   return ret == HAL_OK;
   }
bool I2cPresent_Display(int address) {
   uint8_t ret = HAL_I2C_IsDeviceReady(&hi2c_display, (uint16_t)(address << 1), 3, 5);
   return ret == HAL_OK;
   }


void I2cScan() {
   int count = 0;
   int addr0 = -1, addr1 = -1, addr2 = -1, addr3 = -1, addr4 = -1, addr5 = -1;
   for (int i = 1; i < 128; i++)
      {
      uint8_t ret = HAL_I2C_IsDeviceReady(&hi2c_pmbus, (uint16_t)(i << 1), 3, 5);
      if (ret == HAL_OK)
         {
         count++;
         addr5 = addr4;
         addr4 = addr3;
         addr3 = addr2;
         addr2 = addr1;
         addr1 = addr0;
         addr0 = i;
         }
      }
   sprintf(display0_string, "i2c scan found %d", count);
   sprintf(display1_string, "%x,%x,%x,%x,%x,%x", addr0, addr1, addr2, addr3, addr4, addr5);
   }


void I2C_Transmit(I2C_HandleTypeDef &hi2c, uint8_t address, uint8_t* data, int len)
   {
   int timeout = 1000000;
   hi2c.Instance->CR2 = 
      (len << 16) |
      (1 << 13) | // start
      (1 << 25) | // autoend
      (0 << 10) | // write
      (address << 1) |
      (hi2c.Instance->CR2 & 0xf8004000);
   for (int i = 0; i < len; i++) {
      while (((~hi2c.Instance->ISR) & 2)&&timeout)
         timeout--; // wait for txis to be 1
      hi2c.Instance->TXDR = data[i];
      }
   while (((~hi2c.Instance->ISR) & 0x20) && timeout)
      timeout--; // wait for stop to be 1
   hi2c.Instance->ICR = 0x20;  // clear stop
   hi2c.Instance->CR2 = (hi2c.Instance->CR2 & 0xf8004000); // clear everything
   if (timeout<=0) HAL_I2C_Init(&hi2c);
   }
void I2C_Receive(I2C_HandleTypeDef& hi2c, uint8_t address, uint8_t* data, int len)
   {
   int timeout = 1000000;
   hi2c.Instance->CR2 =
      (len << 16) |
      (1 << 13) | // start
      (1 << 25) | // autoend
      (1 << 10) | // read
      (address << 1) |
      (hi2c.Instance->CR2 & 0xf8004000);
   for (int i = 0; i < len; i++) {
      while (((~hi2c.Instance->ISR) & 4) && timeout)
         timeout--; // wait for rxne to be 1
      data[i] = hi2c.Instance->RXDR;
      }
   while (((~hi2c.Instance->ISR) & 0x20) && timeout)
      timeout--; // wait for stop to be 1
   hi2c.Instance->ICR = 0x20;  // clear stop
   if (timeout <= 0) HAL_I2C_Init(&hi2c);
   }

unsigned char Pec(unsigned char accum, unsigned char data)
   {
   int k;
   accum = accum ^ data;
   for (k = 0; k < 8; k++)
      {
      if (accum & 0x80) accum = (accum << 1) ^ 0x7;
      else accum = accum << 1;
      }
   return accum;
   }

void SMB_Read(I2C_HandleTypeDef& hi2c, uint8_t address, uint8_t* data, int len)
   {
   int timeout = 1000000;
   hi2c.Instance->CR2 =
      (1 << 16) |
      (1 << 13) | // start
      (0 << 25) | // no autoend
      (0 << 10) | // read
      (address << 1) |
      (hi2c.Instance->CR2 & 0xf8004000);
   for (int i = 0; i < 1; i++) {
      while (((~hi2c.Instance->ISR) & 2) && timeout)
         timeout--; // wait for txis to be 1
      hi2c.Instance->TXDR = data[i];
      }
   while (((~hi2c.Instance->ISR) & 0x40) && timeout)
      timeout--; // wait for tc to be 1
   hi2c.Instance->CR2 =
      (len << 16) |
      (1 << 13) | // start
      (1 << 25) | // autoend
      (1 << 10) | // read
      (address << 1) |
      (hi2c.Instance->CR2 & 0xf8004000);
   for (int i = 0; i < len; i++) {
      while (((~hi2c.Instance->ISR) & 4) && timeout)
         timeout--; // wait for rxne to be 1
      data[i] = hi2c.Instance->RXDR;
      }
   while (((~hi2c.Instance->ISR) & 0x20) && timeout)
      timeout--; // wait for stop to be 1
   hi2c.Instance->ICR = 0x20;  // clear stop
   if (timeout <= 0) HAL_I2C_Init(&hi2c);
   }
void PmbusWrite0(short command) {
   uint8_t dt[2], crc = Pec(0, psu_address <<1);
   dt[0] = command;
   dt[1] = Pec(crc, dt[0]);
   I2C_Transmit(hi2c_pmbus, psu_address, dt, 1+1);
   }
void PmbusWrite8(short command, unsigned char data) {
   uint8_t dt[3], crc = Pec(0, psu_address << 1);
   dt[0] = command;
   dt[1] = data;
   dt[2] = Pec(Pec(crc, dt[0]), dt[1]);
   I2C_Transmit(hi2c_pmbus, psu_address, dt, 2+1);
   }
void PmbusWrite16(short command, unsigned short data) {
   uint8_t dt[4], crc = Pec(0, psu_address << 1);
   dt[0] = command;
   dt[1] = data & 0xff;
   dt[2] = data >> 8;
   dt[3] = Pec(Pec(Pec(crc, dt[0]), dt[1]), dt[2]);
   I2C_Transmit(hi2c_pmbus, psu_address, dt, 3+1);
   }

bool I2C_Done()
   {
   return scv_value == 0 && io_value == 0 && core_value == 0 && !i2c_busy;
   }
void I2C_Work()
   {
   uint8_t dt[3];
   int i;
#ifdef EVAL_3ASIC_BOARD_PROJECT
   if (scv_value != 0) {
      i2c_busy = true;
      dt[0] = 0x80;
      dt[1] = scv_value;
      scv_value = 0;
      I2C_Transmit(hi2c_dpot, 0x2c, dt, 2);
      I2C_Transmit(hi2c_dpot, 0x2d, dt, 2);
      I2C_Transmit(hi2c_dpot, 0x2e, dt, 2);
      return;
      }
   if (io_value != 0) {
      i2c_busy = true;
      dt[0] = 0x00;
      dt[1] = io_value;
      io_value = 0;
      I2C_Transmit(hi2c_dpot, 0x2c, dt, 2);
      I2C_Transmit(hi2c_dpot, 0x2d, dt, 2);
      I2C_Transmit(hi2c_dpot, 0x2e, dt, 2);
      return;
      }
#endif
#ifdef EVAL_BOARD_PROJECT
   if (scv_value != 0) {
      i2c_busy = true;
      dt[0] = 0x80;
      dt[1] = scv_value;
      scv_value = 0;
      I2C_Transmit(hi2c_dpot, 0x2c, dt, 2);
      return;
      }
   if (io_value != 0) {
      i2c_busy = true;
      dt[0] = 0x00;
      dt[1] = io_value;
      io_value = 0;
      I2C_Transmit(hi2c_dpot, 0x2c, dt, 2);
      return;
      }
#endif
#ifdef HASH3_PROJECT
   scv_value = io_value = 0;
#endif
   if (core_value!=0 && isl_is_configured){
      i2c_busy = true;
      short temp = core_value;
      if (psu_exponent != 0) {
         float scaled = temp * 0.001 * (1 << -psu_exponent);
         temp = scaled;
         }
      core_value = 0;
      PmbusWrite16(0x21, temp);
      return;
      }
   unsigned int tick = HAL_GetTick();
   static unsigned int lastsingletick=0, lastslowtick = 0,lastfasttick=0;

   if (tick != lastsingletick) {
      int i, average1 = 0, average2 = 0, average3 = 0;
      for (i = 0; i < 256; i++) {
         average1 += adc1[i];
         average2 += adc2[i];
         average3 += adc3[i];
         }
      // /256, /2^16, /8(gain)
      boardinfo.core_current = (average1 + boardinfo.zero_offset[0]) * boardinfo.implied_reference * 7.4505806e-9 * 2000.0;
      boardinfo.io_current = (average2 + boardinfo.zero_offset[1]) * boardinfo.implied_reference * 7.4505806e-9 * 1.0;
      boardinfo.scv_current = (average3 + boardinfo.zero_offset[2]) * boardinfo.implied_reference * 7.4505806e-9 * 1.0;
      boardinfo.raw_adc[0] = average1 >> 8;
      boardinfo.raw_adc[1] = average2 >> 8;
      boardinfo.raw_adc[2] = average3 >> 8;
//      boardinfo.raw_adc[0] = adc1[0];
//      boardinfo.raw_adc[1] = adc2[0];
//      boardinfo.raw_adc[2] = adc3[0];
      if (single_ended) {
         // single ended adc have a gain of 4. The trip_current is measured on 100 Ohm sense resistor
         boardinfo.core_voltage = boardinfo.raw_adc[2] / 262144.0 * boardinfo.implied_reference;
         boardinfo.thermal_trip_current = 0.01 * boardinfo.raw_adc[0] / 262144.0 * boardinfo.implied_reference;
         boardinfo.core_current = -1.0;
         boardinfo.scv_current = -1.0;
         }
      else {
         boardinfo.core_voltage = -1.0;
         boardinfo.thermal_trip_current = -1.0;
         }
#ifdef EVAL_3ASIC_BOARD_PROJECT
      boardinfo.io_current = 0;
      boardinfo.scv_current = 0;
#endif
#ifdef HASH3_PROJECT
      boardinfo.io_current = 0;
      boardinfo.scv_current = 0;
      boardinfo.core_current = 0;
#endif
      }
#ifdef EVAL_3ASIC_BOARD_PROJECT
#endif
#ifdef HASH3_PROJECT
   if ((tick >> 10) != (lastfasttick >> 10)) {   // get every 1 seconds
       dt[0] = 0x79;
       SMB_Read(hi2c_pmbus, psu_address, dt, 2);
       boardinfo.faults = 0;// dt[0] + (dt[1] << 8);
       dt[0] = 0x88;
       SMB_Read(hi2c_pmbus, psu_address, dt, 2);
       boardinfo.supply_vin = linear11(dt[1], dt[0]);
       boardinfo.spare[0] = dt[0];
       boardinfo.spare[1] = dt[1];
       dt[0] = 0x89;
       SMB_Read(hi2c_pmbus, psu_address, dt, 2);
       boardinfo.supply_iin = linear11(dt[1], dt[0]);
       dt[0] = 0x8b;
       SMB_Read(hi2c_pmbus, psu_address, dt, 2);
       boardinfo.supply_voltage = linear16(dt[1], dt[0]);
       dt[0] = 0x8c;
       SMB_Read(hi2c_pmbus, psu_address, dt, 2);
       boardinfo.spare[2] = dt[0];
       boardinfo.spare[3] = dt[1];
//       boardinfo.supply_current = boardinfo.supply_current * 7 + linear11(dt[1], dt[0]) * 0.125;  // IIR smoothing
       boardinfo.supply_current = linear11(dt[1], dt[0]);
       boardinfo.supply_current = MINIMUM(boardinfo.supply_current, 999.9);
       boardinfo.supply_current = MAXIMUM(boardinfo.supply_current, -999.9);
       dt[0] = 0x8d;
       SMB_Read(hi2c_pmbus, psu_address, dt, 2);
       boardinfo.supply_temp = linear11(dt[1], dt[0]);
       dt[0] = 0x96;
       SMB_Read(hi2c_pmbus, psu_address, dt, 2);
//       boardinfo.supply_pout = (boardinfo.supply_pout * 7 + linear11(dt[1], dt[0])) * 0.125; // IIR smoothing
       boardinfo.supply_pout = linear11(dt[1], dt[0]);
       dt[0] = 0x97;
       SMB_Read(hi2c_pmbus, psu_address, dt, 2);
       if (aaps_present) {
          float pin = aaps_present ? (dt[1] * 256 + dt[0]) : linear11(dt[1], dt[0]);
          if (pin > 50 && pin < 8000)  // sometimes I get garbage from AA so filter out clearly wrong values
             {
             for (i = 0; i < 16; i++)
                pin_median[i + 1] = pin_median[i];
             pin_median[0] = pin;
             }
          float pin_sort[17];
          for (i = 0; i < 17; i++)
             pin_sort[i] = pin_median[i];
          std::sort(pin_sort + 0, pin_sort + 16);
          boardinfo.supply_pin = pin_sort[17 / 2];
          }
       else {
          boardinfo.supply_pin = linear11(dt[1], dt[0]);
          dt[0] = 0xe2;
          SMB_Read(hi2c_pmbus, psu_address, dt, 2);
          boardinfo.supply_pin += linear11(dt[1], dt[0]);
          }
       lastfasttick = tick;
       return;
       }
#endif
#ifdef EVAL_BOARD_PROJECT
if ((tick >> 12) != (lastfasttick >> 12)) {   // get every 4 seconds
      dt[0] = 0x8b;
      SMB_Read(hi2c_pmbus, psu_address, dt, 2);
      boardinfo.supply_voltage = (dt[0] + ((signed char)dt[1] * 256)) * 0.001;
      dt[0] = 0x97;
      SMB_Read(hi2c_pmbus, psu_address, dt, 2);
      boardinfo.supply_pin = (dt[0] + ((signed char)dt[1] * 256)) * 1.0;
      dt[0] = 0x21;
      SMB_Read(hi2c_pmbus, psu_address, dt, 2);
      boardinfo.supply_setpoint = (dt[0] + ((signed char)dt[1] * 256)) * 0.001;
      dt[0] = 0x79;
      SMB_Read(hi2c_pmbus, psu_address, dt, 2);
      boardinfo.faults = 0;// dt[0] + (dt[1] << 8);
      dt[0] = 0x8c;
      SMB_Read(hi2c_pmbus, psu_address, dt, 2);
      boardinfo.supply_current = (boardinfo.supply_current*7 + (dt[0] + ((signed char)dt[1] * 256)) * 0.1)*0.125;  // IIR smoothing
      dt[0] = 0x96;
      SMB_Read(hi2c_pmbus, psu_address, dt, 2);
      boardinfo.supply_pout = (boardinfo.supply_pout*7 + (dt[0] + ((signed char)dt[1] * 256)) * 1.0)*0.125; // IIR smoothing
      lastfasttick = tick;
      return;
      }
#endif
   if ((tick >> 13) != (lastslowtick >> 13)) {   // get board temp once every 10 second
#ifdef EVAL_BOARD_PROJECT
      dt[0] = 0;
      I2C_Transmit(hi2c_temp, 0x4f, dt, 1);
      I2C_Receive(hi2c_temp, 0x4f, dt, 2);
      float t = ((int)(signed char)dt[0] << 4) | (dt[1] >> 4);
      boardinfo.board_temp = t * 0.0625;
#endif
#ifdef EVAL_3ASIC_BOARD_PROJECT
      dt[0] = 0;
      I2C_Transmit(hi2c_temp, 0x4f, dt, 1);
      I2C_Receive(hi2c_temp, 0x4f, dt, 2);
      float t = ((int)(signed char)dt[0] << 4) | (dt[1] >> 4);
      boardinfo.board_temp = t * 0.0625;
#endif
      lastslowtick = tick;
      return;
      }

   i2c_busy = false;

   static int oled_row = 0, lastchecksum=0;
   if (oled_row < 8) {
      SSD1306_UpdatePartialScreen(oled_row++);
      }
   else {
      int i, checksum = 0;
      for (i = strlen(display0_string) - 1; i >= 0; i--) {
         checksum += display0_string[i];
         checksum = checksum ^ (checksum << 7);
         }
      for (i = strlen(display1_string) - 1; i >= 0; i--) {
         checksum += display1_string[i];
         checksum = checksum ^ (checksum << 7);
         }
      for (i = strlen(display2_string) - 1; i >= 0; i--) {
         checksum += display2_string[i];
         checksum = checksum ^ (checksum << 7);
         }
      if (checksum != lastchecksum && oled_row >= 8) {
         lastchecksum = checksum;
         OLED_Display(display0_string, display1_string, display2_string);
         oled_row = 0;
         }
      }

   return;
   }

void Boco_Init() {
   uint8_t dt[3];
   psu_address = 0x10;
   dt[0] = 0x20;
   SMB_Read(hi2c_pmbus, psu_address, dt, 1);
   psu_exponent = dt[0] | (dt[0] & 0x10 ? 0xf0 : 0x00);

   core_value = 12000;
   PmbusWrite8(0x1, 0); // disable
   PmbusWrite8(0xe7, 1); // immersion mode
   PmbusWrite0(3); // clear faults
   float scaled = core_value * 0.001 * (1 << -psu_exponent);
   PmbusWrite16(0x21, (int)scaled);
   PmbusWrite8(0x1, 0x80); // enable
   HAL_GPIO_WritePin(psu_enable_GPIO_Port, psu_enable_Pin, core_value != 0 ? GPIO_PIN_RESET : GPIO_PIN_SET);

   dt[0] = 0x88;
   SMB_Read(hi2c_pmbus, psu_address, dt, 2);
   boardinfo.supply_vin = linear11(dt[1],dt[0]);
   dt[0] = 0x89;
   SMB_Read(hi2c_pmbus, psu_address, dt, 2);
   boardinfo.supply_iin = linear11(dt[1], dt[0]);
   dt[0] = 0x8d;
   SMB_Read(hi2c_pmbus, psu_address, dt, 2);
   boardinfo.supply_temp = linear11(dt[1], dt[0]);
   dt[0] = 0x97;
   SMB_Read(hi2c_pmbus, psu_address, dt, 2);
   boardinfo.supply_pin = linear11(dt[1], dt[0]);
   dt[0] = 0xe2;
   SMB_Read(hi2c_pmbus, psu_address, dt, 2);
   boardinfo.supply_pin += linear11(dt[1], dt[0]);
   isl_is_configured = true;
   }

void Aaps_Init() {
   uint8_t dt[3];
   psu_address = 0x58;
   dt[0] = 0x20;
   SMB_Read(hi2c_pmbus, psu_address, dt, 1);
   psu_exponent = dt[0] | (dt[0] & 0x10 ? 0xf0 : 0x00);

   PmbusWrite8(0x1, 0); // disable
   PmbusWrite8(0x3, 0); // clear error
   float scaled = core_value * 0.001 * (1 << -psu_exponent);
   PmbusWrite16(0x21, (int)scaled);
   PmbusWrite8(0x1, 0x80); // enable
   HAL_GPIO_WritePin(psu_enable_GPIO_Port, psu_enable_Pin, core_value != 0 ? GPIO_PIN_RESET : GPIO_PIN_SET);

   dt[0] = 0x88;
   SMB_Read(hi2c_pmbus, psu_address, dt, 2);
   boardinfo.supply_vin = linear11(dt[1], dt[0]);
   dt[0] = 0x89;
   SMB_Read(hi2c_pmbus, psu_address, dt, 2);
   boardinfo.supply_iin = linear11(dt[1], dt[0]);
   dt[0] = 0x8d;
   SMB_Read(hi2c_pmbus, psu_address, dt, 2);
   boardinfo.supply_temp = linear11(dt[1], dt[0]);
   dt[0] = 0x97;
   SMB_Read(hi2c_pmbus, psu_address, dt, 2);
   boardinfo.supply_pin = linear11(dt[1], dt[0]);
   isl_is_configured = true;
   }


void ISL_Init() {
   HAL_GPIO_WritePin(psu_enable_GPIO_Port, psu_enable_Pin, GPIO_PIN_RESET);
   PmbusWrite8(0x10, 0);   // enable all writes
   PmbusWrite8(0x1, 0); // disable
   PmbusWrite8(0x2, 0x1f); // honor enable pin
   PmbusWrite0(0x3);  // clear faults
   PmbusWrite16(0x21, core_value);
#ifdef EVAL_BOARD_PROJECT
   PmbusWrite16(0x40, 500);   // set upper limit to 500mV
#endif
#ifdef EVAL_3ASIC_BOARD_PROJECT
   PmbusWrite16(0x40, 1200);   // set upper limit to 1200mV
#endif
   PmbusWrite8(0x41, 0);   // disable ov fault
   PmbusWrite8(0x45, 0);   // disable uv fault
   PmbusWrite8(0x47, 0);   // disable oc fault
   PmbusWrite8(0x50, 0);   // disable ot fault
   PmbusWrite8(0x54, 0);   // disable ut fault
   PmbusWrite8(0x56, 0);   // disable ov fault
   PmbusWrite8(0x58, 0);   // disable uv fault
   PmbusWrite8(0x5a, 0);   // disable uv fault
   PmbusWrite8(0x5c, 0);   // disable uv fault
   PmbusWrite8(0x1, 0x84); // enable
   HAL_GPIO_WritePin(psu_enable_GPIO_Port, psu_enable_Pin, core_value != 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
   uint8_t dt[3];
   dt[0] = 0x88;
   SMB_Read(hi2c_pmbus, psu_address, dt, 2);
   boardinfo.supply_vin = (dt[0] + ((signed char)dt[1] * 256)) * 0.01;
   dt[0] = 0x89;
   SMB_Read(hi2c_pmbus, psu_address, dt, 2);
   boardinfo.supply_iin = (dt[0] + ((signed char)dt[1] * 256)) * 0.01;
   dt[0] = 0x8d;
   SMB_Read(hi2c_pmbus, psu_address, dt, 2);
   boardinfo.supply_temp = (dt[0] + ((signed char)dt[1] * 256)) * 1.0;
   isl_is_configured = true;
   }



void uc_serialtype::Push(const unsigned short& length, const uint8_t* command_char) {
   if (length == 0) return;

   // I need to wait until the dma is done until I can copy into txbuf
   // I won't be able to send because the callback routine has the wrong interrupt priority but
   // once usb quits, the callback will happen and start another dma
   while (__HAL_DMA_GET_COUNTER(huart.hdmatx) != 0)
      ;
   while (!__HAL_UART_GET_FLAG(&huart, UART_FLAG_TXE))
      ;
   while (!__HAL_UART_GET_FLAG(&huart, UART_FLAG_TC))
      ;

   int i;
   for (i = 0; i < length && i < serial_fifo_size; i++)
      txbuf[i] = command_char[i];
   if (huart.gState == HAL_UART_STATE_READY){
      txbytes_yet_to_send = 0;
      HAL_UART_Transmit_DMA(&huart, (uint8_t*)txbuf, length);
   }
   else {
      txbytes_yet_to_send = length;
      }
   }

void uc_serialtype::Pop(unsigned short& length, uint8_t* response_char)
   {
/*   static char buffer[128];
   sprintf(buffer, "rx dma count=%d pointer = %x byte=%c\n\r", huart.hdmarx->Instance->CNDTR,huart.hdmarx->DmaBaseAddress,huart.Instance->RDR);
   Push(strlen(buffer), buffer);
*/
   int head = sizeof(rxbuf) - huart.hdmarx->Instance->CNDTR;

   length = 0;
   if (head < tail) {
      for (; tail < (int)sizeof(rxbuf) && length<MAX_PACKET_SIZE-16; tail++, length++)
         response_char[length] = rxbuf[tail];
      }
   tail = tail % sizeof(rxbuf);
   if (head > tail) {
      for (; tail < head && length < MAX_PACKET_SIZE - 16; tail++, length++)
         response_char[length] = rxbuf[tail];
      }
   }
void HAL_UART_TxCpltCallback(UART_HandleTypeDef * huart)
   {
   huart->gState = HAL_UART_STATE_READY;
   uc_serialtype* serialptr = NULL;
   if (&serial1.huart == huart)
      serialptr = &serial1;
   else if (&serial2.huart == huart)
      serialptr = &serial2;
   else if (&serial3.huart == huart)
      serialptr = &serial3;

   if (serialptr->txbytes_yet_to_send) {
      HAL_UART_Transmit_DMA(huart, (uint8_t*)serialptr->txbuf, serialptr->txbytes_yet_to_send);
      serialptr->txbytes_yet_to_send = 0;
      }
   }

void uc_serialtype::Init(int baud)
   {
   huart.Init.BaudRate = baud;
   huart.Init.WordLength = UART_WORDLENGTH_8B;
   huart.Init.StopBits = UART_STOPBITS_1;
   huart.Init.Parity = UART_PARITY_NONE;
   huart.Init.Mode = UART_MODE_TX_RX;
   huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
   huart.Init.OverSampling = UART_OVERSAMPLING_8;
   HAL_UART_Init(&huart);

   HAL_UART_Receive_DMA(&huart, (uint8_t*)rxbuf, sizeof(rxbuf));
   return;
   }

#ifdef EVAL_BOARD_PROJECT
// sdadc1, channel 4 single ended thermal_current
// sdadc1, channel 6 differential core current(voltage across .5 mOhm
// sdadc2, channel 8 differential IO current(voltage across 1 Ohm
// sdadc3, channel 6 single ended core voltage
// sdadc3, channel 8 differential scv current(voltage across 1 Ohm
extern "C" void HAL_SDADC_ConvCpltCallback(SDADC_HandleTypeDef * hsdadc)
   {
   boardinfo.adc_counter++;
   int ptr = boardinfo.adc_counter & 0xff;
   adc1[ptr] = HAL_SDADC_GetValue(&hsdadc1);
   adc2[ptr] = HAL_SDADC_GetValue(&hsdadc2);
   adc3[ptr] = HAL_SDADC_GetValue(&hsdadc3);
   }
void Adc_Init()
   {
   HAL_DAC_Start(&hdac2, DAC_CHANNEL_1);

   HAL_SDADC_CalibrationStart(&hsdadc1, SDADC_CALIBRATION_SEQ_1);
   HAL_SDADC_CalibrationStart(&hsdadc2, SDADC_CALIBRATION_SEQ_1);
   HAL_SDADC_CalibrationStart(&hsdadc3, SDADC_CALIBRATION_SEQ_1);

   while (HAL_OK != HAL_SDADC_PollForCalibEvent(&hsdadc1, 500) || HAL_OK != HAL_SDADC_PollForCalibEvent(&hsdadc2, 500) || HAL_OK != HAL_SDADC_PollForCalibEvent(&hsdadc3, 500))
      ;

   HAL_SDADC_ConfigChannel(&hsdadc1, SDADC_CHANNEL_6, SDADC_CONTINUOUS_CONV_ON);
   HAL_SDADC_AssociateChannelConfig(&hsdadc1, SDADC_CHANNEL_6, SDADC_CONF_INDEX_0);
   HAL_SDADC_Start_IT(&hsdadc1);

   HAL_SDADC_ConfigChannel(&hsdadc2, SDADC_CHANNEL_8, SDADC_CONTINUOUS_CONV_ON);
   HAL_SDADC_AssociateChannelConfig(&hsdadc2, SDADC_CHANNEL_8, SDADC_CONF_INDEX_0);
   HAL_SDADC_Start(&hsdadc2);

   HAL_SDADC_ConfigChannel(&hsdadc3, SDADC_CHANNEL_8, SDADC_CONTINUOUS_CONV_ON);
   HAL_SDADC_AssociateChannelConfig(&hsdadc3, SDADC_CHANNEL_8, SDADC_CONF_INDEX_0);
   HAL_SDADC_Start(&hsdadc3);
   single_ended = false;
   boardinfo.implied_reference = 3.2;
   }
void Adc_Zero() {
   int i, average1 = 0, average2 = 0, average3 = 0;
   for (i = 0; i < 256; i++) {
      average1 += adc1[i];
      average2 += adc2[i];
      average3 += adc3[i];
      }
   boardinfo.zero_offset[0] = -average1;
   boardinfo.zero_offset[1] = -average2;
   boardinfo.zero_offset[2] = -average3;
   }
void Adc_ChangeChannel(bool _single_ended) {
   if (single_ended == _single_ended) return;
   if (!_single_ended) {
      HAL_SDADC_Stop_IT(&hsdadc1);
      HAL_SDADC_ConfigChannel(&hsdadc1, SDADC_CHANNEL_6, SDADC_CONTINUOUS_CONV_ON);
      HAL_SDADC_AssociateChannelConfig(&hsdadc1, SDADC_CHANNEL_6, SDADC_CONF_INDEX_0);
      HAL_SDADC_Start_IT(&hsdadc1);

      HAL_SDADC_Stop(&hsdadc3);
      HAL_SDADC_ConfigChannel(&hsdadc3, SDADC_CHANNEL_8, SDADC_CONTINUOUS_CONV_ON);
      HAL_SDADC_AssociateChannelConfig(&hsdadc3, SDADC_CHANNEL_8, SDADC_CONF_INDEX_0);
      HAL_SDADC_Start(&hsdadc3);
      }
   else{
      HAL_SDADC_Stop_IT(&hsdadc1);
      HAL_SDADC_ConfigChannel(&hsdadc1, SDADC_CHANNEL_4, SDADC_CONTINUOUS_CONV_ON);
      HAL_SDADC_AssociateChannelConfig(&hsdadc1, SDADC_CHANNEL_4, SDADC_CONF_INDEX_1);
      HAL_SDADC_Start_IT(&hsdadc1);

      HAL_SDADC_Stop(&hsdadc3);
      HAL_SDADC_ConfigChannel(&hsdadc3, SDADC_CHANNEL_6, SDADC_CONTINUOUS_CONV_ON);
      HAL_SDADC_AssociateChannelConfig(&hsdadc3, SDADC_CHANNEL_6, SDADC_CONF_INDEX_1);
      HAL_SDADC_Start(&hsdadc3);
      }
   single_ended = _single_ended;
   }
void Dac2(int value)
   {
   value = value & 0xfff;
   HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R, value);
   }
#endif
#ifdef EVAL_3ASIC_BOARD_PROJECT
// sdadc1, channel 4 single ended thermal_current
// sdadc1, channel 6 differential core current(voltage across .5 mOhm
extern "C" void HAL_SDADC_ConvCpltCallback(SDADC_HandleTypeDef * hsdadc)
   {
   boardinfo.adc_counter++;
   int ptr = boardinfo.adc_counter & 0xff;
   adc1[ptr] = HAL_SDADC_GetValue(&hsdadc1);
   }
void Adc_Init()
   {
   HAL_SDADC_CalibrationStart(&hsdadc1, SDADC_CALIBRATION_SEQ_1);

   while (HAL_OK != HAL_SDADC_PollForCalibEvent(&hsdadc1, 500))
      ;

   HAL_SDADC_ConfigChannel(&hsdadc1, SDADC_CHANNEL_6, SDADC_CONTINUOUS_CONV_ON);
   HAL_SDADC_AssociateChannelConfig(&hsdadc1, SDADC_CHANNEL_6, SDADC_CONF_INDEX_0);
   HAL_SDADC_Start_IT(&hsdadc1);

   single_ended = false;
   boardinfo.implied_reference = 3.2;
   }
void Adc_Zero() {
   int i, average1 = 0;
   for (i = 0; i < 256; i++) {
      average1 += adc1[i];
      }
   boardinfo.zero_offset[0] = -average1;
   }
void Adc_ChangeChannel(bool _single_ended) {}
void Dac2(int value){}
#endif
#ifdef HASH3_PROJECT
void Adc(bool _single_ended) {};
void Adc_Zero() {}
void Dac2(int value) {}
void Adc_ChangeChannel(bool _single_ended) {}
#endif




void LED_Blue(bool enable) {
#ifdef led_blue_GPIO_Port
   HAL_GPIO_WritePin(led_blue_GPIO_Port, led_blue_Pin, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
   }

void LED_Red(bool enable) {
#ifdef led_red_GPIO_Port
   HAL_GPIO_WritePin(led_red_GPIO_Port, led_red_Pin, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
   }


void I2C_Switch(int board)   // board=0,1,2
   {
#ifdef HASH3_PROJECT
   static bool not_first_time = false;

   if (!not_first_time) { // this code will reset the spi switch
      not_first_time = true; 
      HAL_GPIO_WritePin(spi_switch_reset_n_GPIO_Port, spi_switch_reset_n_Pin, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(spi_switch_reset_n_GPIO_Port, spi_switch_reset_n_Pin, GPIO_PIN_SET);
      }

   uint8_t dt[2];
   if (true)
      {
      dt[0] = 0x80 | (3<<(board*2));
      I2C_Transmit(hi2c_display, 0x71, dt, 1);   // original board
      }
   else {
      dt[0] = 0x88 | (0x11<<(board));
      I2C_Transmit(hi2c_display, 0x70, dt, 1);
      }
#endif
   }

void SCV_Supply(int millivolts) {
#ifdef EVAL_BOARD_PROJECT
   HAL_GPIO_WritePin(scv_enable_GPIO_Port, scv_enable_Pin, millivolts != 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
   // linear regulator has a 200mV reference voltage. Digital pot is voltage divider with 256 steps
   int best = 255;
   float error = 1.0e6;

//   if (millivolts > 1000) millivolts = 1000;

   for (int i = 0; i <= 255; i++) {
      float rwb = 78 * i + 60;
      float v = 200.0 / (rwb / 20000.0);
      if (fabs(v- millivolts) < error) {
         error = fabs(v - millivolts);
         best = i;
         }
      }
   scv_value = best;
   scv_millivolts = millivolts==0 ? 0 : 200.0 / ((78 * best + 60) / 20000.0);
   }

void IO_Supply(int millivolts) {
#ifdef EVAL_BOARD_PROJECT
   HAL_GPIO_WritePin(io_enable_GPIO_Port, io_enable_Pin, millivolts != 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
   // linear regulator has a 200mV reference voltage. Digital pot is voltage divider with 256 steps
   int best = 255;
   float error = 1.0e6;

//   if (millivolts > 1400) millivolts = 1400;

   for (int i = 0; i <= 255; i++) {
      float rwb = 78 * i + 60;
      float v = 200.0 / (rwb / 20000.0);
      if (fabs(v- millivolts) < error) {
         error = fabs(v - millivolts);
         best = i;
         }
      }
   io_value = best;
   io_millivolts = millivolts == 0 ? 0 : 200.0 / ((78 * best + 60) / 20000.0);
   }

void CORE_Supply(int millivolts) {
#ifdef psu_enable_GPIO_Port
 #ifdef HASH3_PROJECT
   HAL_GPIO_WritePin(psu_enable_GPIO_Port, psu_enable_Pin, millivolts != 0 ? GPIO_PIN_RESET : GPIO_PIN_SET);
 #else
   HAL_GPIO_WritePin(psu_enable_GPIO_Port, psu_enable_Pin, millivolts != 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
 #endif
#endif
#ifdef EVAL_BOARD_PROJECT
   if (millivolts > 700) millivolts = 700;
#endif
   core_value = millivolts;
   core_millivolts = millivolts;
   }


void ssd1306_I2C_Write(uint8_t address, uint8_t reg, uint8_t data) {
   uint8_t dt[2];
   dt[0] = reg;
   dt[1] = data;
   I2C_Transmit(hi2c_display, address, dt, 2);
   }
void ssd1306_I2C_Write_Multi(uint8_t address, uint8_t reg, uint8_t* ptr, int len)
   {
   uint8_t dt[130];
   int i;
   dt[0] = reg;
   for (i = 0; i < len && i<(int)sizeof(dt); i++)
      dt[i + 1] = ptr[i];
   I2C_Transmit(hi2c_display, address, dt, i);
   }

void PWM_fan(int value) {
#ifdef HASH3_PROJECT
   fan_pwm_setting = value;
   __HAL_TIM_SET_COMPARE(&htim19, TIM_CHANNEL_2, (htim19.Init.Period * value)>>8);
   HAL_TIM_PWM_Start(&htim19, TIM_CHANNEL_2);
#endif
   }

void SetGPIO(const gpiotype& gpio)
   {
#ifdef id33_0__GPIO_Port
   HAL_GPIO_WritePin(id33_0__GPIO_Port, id33_0__Pin, (gpio.id & 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef id33_1__GPIO_Port
   HAL_GPIO_WritePin(id33_1__GPIO_Port, id33_1__Pin, (gpio.id & 2) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef id33_2__GPIO_Port
   HAL_GPIO_WritePin(id33_2__GPIO_Port, id33_2__Pin, (gpio.id & 4) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef id33_3__GPIO_Port
   HAL_GPIO_WritePin(id33_3__GPIO_Port, id33_3__Pin, (gpio.id & 8) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef id33_4__GPIO_Port
   HAL_GPIO_WritePin(id33_4__GPIO_Port, id33_4__Pin, (gpio.id & 16) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef id33_5__GPIO_Port
   HAL_GPIO_WritePin(id33_5__GPIO_Port, id33_5__Pin, (gpio.id & 32) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef id33_6__GPIO_Port
   HAL_GPIO_WritePin(id33_6__GPIO_Port, id33_6__Pin, (gpio.id & 64) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef id33_7__GPIO_Port
   HAL_GPIO_WritePin(id33_7__GPIO_Port, id33_7__Pin, (gpio.id & 128) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef response_i_GPIO_Port
   HAL_GPIO_WritePin(response_i_GPIO_Port, response_i_Pin, gpio.response_i ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
//   HAL_GPIO_WritePin(test_clockout_GPIO_Port, test_clockout_Pin, gpio.test_mode ? GPIO_PIN_SET : GPIO_PIN_RESET);
#ifdef thermal_trip_i33_GPIO_Port
   HAL_GPIO_WritePin(thermal_trip_i33_GPIO_Port, thermal_trip_i33_Pin, gpio.thermal_trip ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef reset_i33_GPIO_Port
   HAL_GPIO_WritePin(reset_i33_GPIO_Port, reset_i33_Pin, gpio.reset ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef reset_n_GPIO_Port
   HAL_GPIO_WritePin(reset_n_GPIO_Port, reset_n_Pin, gpio.reset ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef enable_25mhz_GPIO_Port
   HAL_GPIO_WritePin(enable_25mhz_GPIO_Port, enable_25mhz_Pin, gpio.enable_25mhz ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
#ifdef enable_100mhz_GPIO_Port
   HAL_GPIO_WritePin(enable_100mhz_GPIO_Port, enable_100mhz_Pin, gpio.enable_100mhz ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
   }

unsigned short ReadGPIO()
   {
   unsigned short input_pins = 0;
#ifdef thermal_trip_o33_GPIO_Port
   input_pins |= HAL_GPIO_ReadPin(thermal_trip_o33_GPIO_Port, thermal_trip_o33_Pin) == GPIO_PIN_SET ? 1 : 0;
#endif
#ifdef test_clock_out_Port
   input_pins |= HAL_GPIO_ReadPin(test_clock_out_Port, test_clock_out_Pin) == GPIO_PIN_SET ? 2 : 0;
#endif
   return input_pins;
   }

/*
short ReadSwitches() {
   short result = 0;
   result += HAL_GPIO_ReadPin(sw1_GPIO_Port, sw1_Pin) == GPIO_PIN_SET ? 1 : 0;
   result += HAL_GPIO_ReadPin(sw2_GPIO_Port, sw2_Pin) == GPIO_PIN_SET ? 2 : 0;
   result += HAL_GPIO_ReadPin(sw3_GPIO_Port, sw3_Pin) == GPIO_PIN_SET ? 4 : 0;
   result += HAL_GPIO_ReadPin(sw4_GPIO_Port, sw4_Pin) == GPIO_PIN_SET ? 8 : 0;
   return result;
   }
*/

extern "C" void DebugString(const char*txt) {
   const char* chptr = txt;
   while (*chptr != 0) {
      ITM_SendChar(*chptr);
      chptr++;
      }
   }

