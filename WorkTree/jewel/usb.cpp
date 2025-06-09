#include "pch.h"
#include "helper.h"
#include "../uctl_common/eval_board.h"
#include "usb.h"

void libusbLog(libusb_context* ctx, enum libusb_log_level level, const char* str)
   {
   printf("%s\n", str);
   }

void usbtype::Transaction() {
   int ret, actual, sendlen;

   if (handle == NULL) FATAL_ERROR; // call init first
   
   uc.unique = 0x8dac;
   sendlen = sizeof(uc) + uc.length;
   if (sendlen > MAX_PACKET_SIZE) FATAL_ERROR;
   memset(responsebuf, 0, sizeof(responsebuf));
//   ret = libusb_bulk_transfer(handle, 0X01, (unsigned char*)&uc, 0, &actual, timeout);
   ret = libusb_bulk_transfer(handle, 0X01, (unsigned char*)&uc, sendlen, &actual, timeout);
   if (ret < 0 || actual != sendlen) {
      printf("Error accessing Auradine through USB\nUnplug the USB cable and plug back in\n");
      exit(-1);
      FATAL_ERROR;
      }
   actual = -1;
   ret = libusb_bulk_transfer(handle, 0X81, responsebuf, MAX_PACKET_SIZE, &actual, timeout);
   if (ret < 0) {
      printf("Usb error, no response. Retrying...\n ");
      Sleep(1000); // retry any see if response is ready
      ret = libusb_bulk_transfer(handle, 0X01, (unsigned char*)&uc, sendlen, &actual, timeout);
      ret = libusb_bulk_transfer(handle, 0X81, responsebuf, MAX_PACKET_SIZE, &actual, timeout);
      }
   if (ret < 0 || actual < sizeof(ur)) FATAL_ERROR;
   if (ur.unique != 0x321f && ur.unique != 0x321e) FATAL_ERROR;
   if (ur.command != uc.command)
      {
      printf("The usb response didn't match the command, ur.unique=%x, ur.command=%x\n",ur.unique, ur.command);
      FATAL_ERROR;
      }
   if (actual != sizeof(ur) + ur.length) FATAL_ERROR;
   if (ur.unique == 0x321e && uc.command != UI_ERRORSTRING)
      CheckForError();
   }

void usbtype::ClearUsb() {
   int ret, actual, sendlen;

   if (handle == NULL) FATAL_ERROR; // call init first

   uc.unique = 0x8dac;
   uc.command = UI_VERSION;
   uc.length = 0;
   sendlen = sizeof(uc) + uc.length;
   if (sendlen > MAX_PACKET_SIZE) FATAL_ERROR;
   memset(responsebuf, 0, sizeof(responsebuf));
   //   ret = libusb_bulk_transfer(handle, 0X01, (unsigned char*)&uc, 0, &actual, timeout);
   ret = libusb_bulk_transfer(handle, 0X01, (unsigned char*)&uc, sendlen, &actual, timeout);
   if (ret < 0 || actual != sendlen) {
      printf("Error accessing Auradine through USB\nUnplug the USB cable and plug back in\n");
      exit(-1);
      FATAL_ERROR;
      }
   actual = -1;
   ret = libusb_bulk_transfer(handle, 0X81, responsebuf, MAX_PACKET_SIZE, &actual, timeout);
   }

void usbtype::Init()
   {
   libusb_init(&ctx);
//      libusb_set_debug(ctx, LIBUSB_LOG_LEVEL_DEBUG);
   libusb_set_log_cb(ctx, libusbLog, LIBUSB_LOG_CB_GLOBAL);

   libusb_device** list;
   int i;
   int deviceCount = libusb_get_device_list(ctx, &list);
   for (i = deviceCount - 1; i >= 0; i--) {
      struct libusb_device* device = list[i];
      struct libusb_device_descriptor desc;
      libusb_get_device_descriptor(device, &desc);
      if (desc.idVendor == USBD_VID && desc.idProduct == USBD_PID_FS) {
         libusb_open(device, &handle);
         if (handle)
            break;
         }
      }
   if (i < 0) {
      printf("Couldn't find Auradine eval board on USB\n");
      FATAL_ERROR;
      }
/*
   //Open Device with VendorID and ProductID
   handle = libusb_open_device_with_vid_pid(ctx, USBD_VID, USBD_PID_FS);
   if (!handle) {
      printf("Couldn't find Auradine eval board on USB\n");
      FATAL_ERROR;
      }
*/
   int r = libusb_claim_interface(handle, 1);
   //Claim Interface 1 from the device
   if (r < 0) {
      printf("usb_claim_interface error %s\n", libusb_strerror(r));
      FATAL_ERROR;
      }

   /*
   int ret, actual = 0;
   uc.command = UI_NOTHING;
   uc.unique = 0x8dac;
   uc.length = 0;
   uc.extra = 0;
   ret = libusb_bulk_transfer(handle, 0X01, (unsigned char*)&uc, sizeof(uc), &actual, timeout);
   if (ret < 0)
      {
      libusb_release_interface(handle, r);
      libusb_reset_device(handle);
      libusb_close(handle);
      libusb_exit(NULL);
      printf("Usb interface not working. Try again. If it still doesn't work unplug/reinsert the usb cable\n");
      exit(-1);
      }*/
   ClearUsb();
   Version();
   }

void usbtype::BoardInfo(boardinfotype &info, bool quiet)
   {
   uc.command = UI_BOARDINFO;
   uc.length = 0;
   Transaction();
   if (ur.length != sizeof(info)) FATAL_ERROR;
   memcpy(&info, response_char, sizeof(info));
   if (quiet) return;
   printf("\n");
   printf("Board Temp\t%6.3f C\n", info.board_temp);
   printf("Supply Temp\t%6.3f C\n", info.supply_temp);
   printf("Supply Vin\t%6.3f V\n", info.supply_vin);
   printf("Supply Iin\t%6.3f A\n", info.supply_iin);
   printf("Supply Pin\t%6.3f W\n", info.supply_pin);
   printf("Supply Vout\t%6.3f V\n", info.supply_voltage);
   printf("Supply Iout\t%6.3f A\n", info.supply_current);
   printf("Supply Pout\t%6.3f W\n", info.supply_pout);
   printf("IO Current\t%6.2f mA\n", info.io_current * 1000.0);
   printf("SCV Current\t%6.2f mA\n", info.scv_current * 1000.0);
   printf("Core Current\t%6.3f A\n", info.core_current);
   printf("Core Voltage\t%6.3f V\n", info.core_voltage);
   printf("Pad current\t%6.3f mA\n", info.thermal_trip_current * 1000.0);
   }

void usbtype::VddEnable(int board_masks)
   {
   uc.command = UI_ENABLE;
   uc.length = 0;
   uc.extra = board_masks;   // 0,1,2 is vdd_enables
   Transaction();
   }

void usbtype::SetBoard(int board)
   {
   if (board < 0 || board>2) FATAL_ERROR;
   uc.command = UI_BOARD;
   uc.length = 0;
   uc.extra = 0x101 << board;   // 0,1,2 is rx_enables, 0x100,0x200,0x400 is tx_enables
   Transaction();
   lastboard = board;
   Sleep(1);
   }
void usbtype::SetBaudRate(int baud)
   {
   uc.command = UI_SETBAUD;
   uc.length = 4;
   memcpy(command_char, &baud, 4);
   Transaction();
   }
void usbtype::SetVoltage(supplytype port, int millivolt)  // 0=core, 1=scv, 2=io
   {
   int count = 50000;
   uc.command = UI_SUPPLY;
   uc.length = 4;
   uc.extra = port;
   memcpy(command_char, &millivolt, 4);
   Transaction();
   while (true) {
      uc.command = UI_I2C_DONE;
      uc.length = 0;
      Transaction();
      if (ur.extra || count<=0) break;
      count--;
      }
   if (count <= 0) { printf("Timeout waiting for i2c to communicate\n"); }
   }
void usbtype::SetCurrentLimit(int amps)
   {
   uc.command = UI_SET_OVERCURRENT_LIMIT;
   uc.length = 0;
   uc.extra = amps;
   Transaction();
   }
void usbtype::SetFanSpeed(int speed)
   {
   uc.command = UI_FAN;
   uc.length = 0;
   uc.extra = speed;
   Transaction();
   }
void usbtype::ZeroADC() {
   uc.command = UI_ZERO_ADC;
   uc.length = 0;
   Transaction();
   }

float usbtype::ReadPower()
   {
   boardinfotype info;
   uc.command = UI_BOARDINFO;
   uc.length = 0;
   Transaction();
   if (ur.length != sizeof(info)) FATAL_ERROR;
   memcpy(&info, response_char, sizeof(info));
   return info.supply_pin;
//   return info.supply_vin * info.supply_iin;
    }
void usbtype::Analog(bool single_ended, int dac)
   {
   uc.command = UI_ANALOG;
   uc.length = 0;
   uc.extra = (single_ended ? 1 : 0) | (dac << 4);
   Transaction();
   }
void usbtype::SetImmersion(bool enable)
   {
   uc.command = UI_ENABLE_DISABLE_IMMERSION;
   uc.length = 0;
   uc.extra = enable ?1:0;
   Transaction();
   }
void usbtype::OLED_Display(const char* txt)
   {
   uc.command = UI_OLED;
   strcpy(command_char, txt);
   uc.length = strlen(txt);
   Transaction();
   }
void usbtype::Debug()
   {
   uc.command = UI_DEBUG;
   uc.length = 0;
   uc.extra = 0;
   Transaction();
   for (int i = 0; i < ur.length / 4; i++)
      printf("Debug[%d]=%x\n", i, response_int[i]);
   }
void usbtype::DrainReceiver()
   {
   do {
      uc.command = UI_SERIAL;
      uc.length = 0;
      Transaction();
      } while (ur.length != 0);
   }
void usbtype::SendData(int board, const char* data, int len)
   {
   int ptr = 0;
   if (len <= 0) return;
   if (board < 0) board = lastboard;
   if (board < 0 || board>2) FATAL_ERROR;
   if (board != lastboard) SetBoard(board);
   for (ptr = 0; ptr < len; ) {
      int chunk = MINIMUM(len - ptr, MAX_PACKET_SIZE - 16);
      uc.command = UI_SERIAL;
      uc.length = chunk;
      if (chunk > MAX_PACKET_SIZE - sizeof(usbcommandtype)) FATAL_ERROR;
      memcpy(command_char, data + ptr, chunk);
      Transaction();
      ptr += chunk;
      for (int i = 0; i < ur.length; i++)
         received_data.push_back(response_char[i]);
      }
   }
void usbtype::RcvData(int board, void* data, int rcvsize, bool quiet)
   {
   int timeout = 0;
   if (board != lastboard) FATAL_ERROR;
//   if (board != lastboard) SetBoard(board);
   while (true) {
      if (received_data.size() >= rcvsize || timeout>1000)
         {
         int i;
//         printf("%d/%d bytes received.  \n", received_data.size(), rcvsize);
         for (i = 0; i < rcvsize && i<received_data.size(); i++) {
            ((char*)data)[i] = received_data[i];
            }
         received_data.erase(received_data.begin(), received_data.begin()+i);
         if (!quiet && timeout > 1000) printf("Usb timed out receiving data\n");
         return;
         }
      uc.command = UI_SERIAL;
      uc.length = 0;
      Transaction();
      for (int i = 0; i < ur.length; i++) {
         received_data.push_back(response_char[i]);
         timeout = 0;
         }
      timeout++;
      }
   }

void usbtype::LoopBackTest()
   {
   int i;
   const int bufsize = 50000;
   vector<char> send(bufsize);
   vector<char> rcv(bufsize);
   int seed = 0;
   struct _timeb start, end;

   for (i = 0; i < send.size(); i++)
      {
      send[i] = random(seed) * 256;
      }
   SetBaudRate(9000000);
   DrainReceiver();
   _ftime(&start);
   for (i = 0; i < 10; i++) {
      SendData(0, &send[0], send.size());
      memset(&rcv[0], 0, rcv.size());
      RcvData(0, &rcv[0], rcv.size());
      for (int k = 0; k < rcv.size(); k++)
         if (send[k] != rcv[k])
            printf("Mismatch at %d, %x!=%x\n",k,send[k],rcv[k]);
//      if (memcpy(&rcv[0], &send[0], rcv.size()) != 0)
//         printf("Error\n");
      }
   _ftime(&end);
   float elapsed = end.time + end.millitm*0.001 - start.time - start.millitm * 0.001;
   printf("Effective baudrate was %.0f\n",(float)rcv.size() *i*10/elapsed);
   }


void usbtype::CheckForError()
   {
   uc.command = UI_ERRORSTRING;
   uc.length = 0;
   Transaction();

   if (ur.length)
      printf("A ucontroller error string exists(%d bytes): %s\n", ur.length, response_char);
   }

char* usbtype::Version()
   {
   static char info[256];

   uc.command = UI_VERSION;
   uc.length = 0;
   Transaction();

   printf("Auradine ucontroller has been found.\t%s\n", response_char);
   strcpy(info, response_char);
   if (strstr(info, "1asic") != NULL) version = 1;
   else if (strstr(info, "3asic") != NULL) version = 3;
   else if (strstr(info, "Hash") != NULL) version = 2;
   else if (strstr(info, "Comm") != NULL) version = 4;
   else version = 0; // unknown
   return info;
   }

unsigned short usbtype::ReadWriteGPIO(gpiotype gpio) {
   gpiouniontype g;
   g.gpio = gpio;
   uc.command = UI_GPIO_READWRITE;
   uc.length = 0;
   uc.extra = g.s;
   Transaction();
   return ur.extra;  // [0]=thermal_trip, [1]=test_clockout
   }

