#ifndef __USB_H_INCLUDED__
#define __USB_H_INCLUDED__

#include "../libusb/libusb.h"
#include "../uctl_common/eval_board.h"


#define USBD_VID        1155
#define USBD_PID_FS     0x1dac

void libusbLog(libusb_context* ctx, enum libusb_log_level level, const char* str);

enum supplytype;

class usbtype {
private:
   
   libusb_context* ctx;
   libusb_device_handle* handle;
   const int timeout;
   int lastboard;

   uint8_t commandbuf[MAX_PACKET_SIZE];
   uint8_t responsebuf[MAX_PACKET_SIZE];
   usbcommandtype& uc;  // = *(usbcommandtype*)commandbuf;
   usbresponsetype& ur; // = *(usbresponsetype*)responsebuf;
   char* response_char; // = (char*)responsebuf + sizeof(usbresponsetype);
   int* response_int; // = (char*)responsebuf + sizeof(usbresponsetype);
   char* command_char; // = (char*)responsebuf + sizeof(usbresponsetype);
   vector<char> received_data;


   void Transaction();
   void ClearUsb();

public:
   int version;   // 1=1asic, 3=3 asic, 2=hash board
   int totalPackets;

   usbtype() : timeout(100), uc(*(usbcommandtype*)commandbuf), ur(*(usbresponsetype*)responsebuf), response_char((char*)responsebuf + sizeof(usbresponsetype)), response_int((int*)((char*)responsebuf + sizeof(usbresponsetype))), command_char((char*)commandbuf + sizeof(usbcommandtype))
      {
      ctx = NULL;
      handle = NULL;
      version = 0;
      lastboard = -1;
      }
   void Init();
   ~usbtype() {
      if (handle!=NULL)
         libusb_close(handle);
      if (ctx!=NULL)
         libusb_exit(NULL);
      }
   void CheckForError();
   char *Version();
   void LoopBackTest();
   unsigned short ReadWriteGPIO(gpiotype gpio);
   void SetBoard(int board);
   void SetBaudRate(int baud);
   void SendData(int board, const char* data, int len);
   void RcvData(int board, void* data, int rcvsize, bool quiet = false);
   void DrainReceiver();
   void OLED_Display(const char *txt);
   void SetVoltage(supplytype port, int millivolt);  // 0=core, 1=scv, 2=io
   void SetCurrentLimit(int amps);
   void SetImmersion(bool enable);
   void ZeroADC();
   void VddEnable(int board_masks);
   void SetFanSpeed(int speed);
   float ReadPower();
   void Debug();
   void Analog(bool single_ended, int dac);
   void BoardInfo(boardinfotype& info, bool quiet = false);
   };




#endif //__USB_H_INCLUDED__


