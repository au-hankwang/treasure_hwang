#ifndef __SERIAL_H_INCLUDED__
#define __SERIAL_H_INCLUDED__

class serialtype {
private:
   HANDLE hComm;
   DCB dcb;
   COMMTIMEOUTS ct;

public:
   int totalBytesRead, totalBytesWritten;

   serialtype()
      {
      hComm = NULL;
      }
   ~serialtype() {
      if (hComm != NULL)
         CloseHandle(hComm);
      hComm = NULL;
      }

   void Init(const char* port = "COM7") {
      if (port == NULL) return;
      hComm = CreateFile(port, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
      if (hComm == INVALID_HANDLE_VALUE) {
         printf("I couldn't open '%s' This driver will not function!!!", port);
         FATAL_ERROR;
         }

      SetBaudRate(115200);

      SetupComm(hComm, 1024 * 8, 1024 * 8);

      memset(&ct, 0, sizeof(ct));
      ct.ReadTotalTimeoutConstant = 100;  // 0.01 second timeout value for reads
      ct.ReadTotalTimeoutConstant = 2000;  // fixme
      SetCommTimeouts(hComm, &ct);
      totalBytesWritten = 0;
      totalBytesRead = 0;
      }
   void SetBaudRate(int baud) {
      memset(&dcb, 0, sizeof(dcb));
      dcb.DCBlength = sizeof(dcb);
      dcb.BaudRate = baud;

      dcb.fBinary = TRUE;
      //      dcb.fDtrControl = DTR_CONTROL_ENABLE;  // I don't use handshaking but setting these will help with a striaght through cable (ie no null modem)
      //      dcb.fRtsControl = RTS_CONTROL_ENABLE;
      dcb.ByteSize = 8;
      dcb.StopBits = ONESTOPBIT;
      dcb.Parity = NOPARITY;

      //      printf("Setting PC buadrate to %d\n", baud);
      if (!SetCommState(hComm, &dcb)) {
         printf("I encountered an error setting the baud rate!");
         printf("GetLastError() is 0x%X", (int)GetLastError());
         //         FATAL_ERROR;
         }

      }
   void SendByte(unsigned char data) {
      unsigned char buffer[16];
      DWORD bytesWritten = 0;

      buffer[0] = data;
      WriteFile(hComm, &buffer, 1, &bytesWritten, NULL);
      totalBytesWritten += bytesWritten;
      }
   void SendData(void* data, int len) {
      DWORD bytesWritten = 0;

      WriteFile(hComm, data, len, &bytesWritten, NULL);
      totalBytesWritten += bytesWritten;
      }
   void RcvByte(unsigned char& data) {
      unsigned char buffer[16];
      DWORD bytesRead = 0;
      unsigned int rcvsize = 1;

      ReadFile(hComm, buffer, rcvsize, &bytesRead, NULL);
      if (rcvsize != bytesRead)
         printf("\nReceive timeout");
      data = buffer[0];
      totalBytesRead += bytesRead;
      }
   int RcvData(void* data, int rcvsize, bool quiet=false) {
      DWORD bytesRead = 0;

      ReadFile(hComm, data, rcvsize, &bytesRead, NULL);
      if (rcvsize != bytesRead && !quiet)
         printf("\nReceive timeout (%d/%d bytes)\n", bytesRead, rcvsize);
      totalBytesRead += bytesRead;
      return bytesRead;
      }
   void DrainReceiver() {
      DWORD bytesRead = 0;
      char data[1024];

      while (true) {
         ReadFile(hComm, data, sizeof(data), &bytesRead, NULL);
         if (sizeof(data) != bytesRead)
            break;
         }
      }
   };




#endif //__SERIAL_H_INCLUDED__


