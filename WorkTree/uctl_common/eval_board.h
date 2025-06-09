/*
 * eval_board.h
 *
 *  Created on: Jan 24, 2023
 *      Author: dcarlson
 */

#ifndef INC_EVAL_BOARD_H_
#define INC_EVAL_BOARD_H_

#define MINIMUM(a,b) ( ((a)<(b)) ? (a) : (b))
#define MAXIMUM(a,b) ( ((a)>(b)) ? (a) : (b))


const int MAX_PACKET_SIZE = 512;

struct gpiotype { // these are the gpio output of the micro controller
   unsigned short id : 8;
   unsigned short reset : 1;
   unsigned short test_mode : 1;
   unsigned short thermal_trip : 1;
   unsigned short response_i : 1;
   unsigned short enable_25mhz : 1;
   unsigned short enable_100mhz : 1;
   unsigned short spare : 2;
   void clear() {
      id = 0;
      reset = 0;
      test_mode = 0;
      thermal_trip = 0;
      response_i = 0;
      enable_25mhz = 0;
      enable_100mhz = 0;
      spare = 0;
      }
   };
union gpiouniontype {
   gpiotype gpio;
   short s;
   };

struct boardinfotype {
   float supply_setpoint, supply_voltage, supply_current, supply_pin, supply_pout, supply_temp, supply_vin, supply_iin;
   short faults;
   float board_temp;
   float scv_current, io_current, core_current;
   float core_voltage, thermal_trip_current, ro_current;
   int raw_adc[3], zero_offset[3];
   int adc_counter;
   int boards_present;  // [0]=board0, [1]=board1, [2]=board2
   int spare[7];
   };

enum supplytype {
   S_CORE = 0, S_SCV = 1, S_IO = 2, S_RO = 3, S_ZERO=4
   };

enum ui_commandtype {
   UI_NOTHING,
   UI_VERSION,
   UI_ERRORSTRING,
   UI_DEBUG,
   UI_GPIO_READWRITE,
   UI_SETBAUD,
   UI_SERIAL,
   UI_SUPPLY,
   UI_OLED,
   UI_BOARD,
   UI_BOARDINFO,
   UI_ENABLE,     // [0]=vdd_enable1, [1]=vdd_enable2, [2]=vdd_enable3,
   UI_FAN,        // extra=0-255
   UI_ANALOG,     // extra[0]=0 for differential current measurements, 1=single ended, [15:4]=dac2 output
   UI_I2C_DONE,
   UI_ZERO_ADC,
   UI_SET_OVERCURRENT_LIMIT,      // extra will be limit in A
   UI_ENABLE_DISABLE_IMMERSION      // [0]=enable
   };

struct usbcommandtype {
   unsigned short unique;       // should be 0x8dac
   unsigned short command;
   unsigned short extra;
   unsigned short length;
   usbcommandtype() {
      unique = 0x8dac;
      command = UI_NOTHING;
      extra = 0;
      length = 0;
      }
   };

struct usbresponsetype {
   unsigned short unique;       // should be 0x321f, 0x321e for error
   unsigned short command;
   unsigned short extra;
   unsigned short length;        // number of bytes following this structure
   usbresponsetype() {
      unique = 0x321f;
      command = UI_NOTHING;
      extra = 0;
      length = 0;
      }
   };



#endif /* INC_EVAL_BOARD_H_ */
