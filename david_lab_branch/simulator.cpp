#include "pch.h"
#include "sha256.h"
#include "helper.h"
#include "miner.h"
#include "dvfs.h"
#include "electrical.h"
#include "serial.h"



struct topologySorttype { // function object (predicate for STL)
   const int personality;
   topologySorttype(int _personality) : personality(_personality) {}
   bool operator()(const topologytype& left, const topologytype& right) const
      {
      if (personality == 1) {
         if (left.board < right.board) return true;
         if (left.board > right.board) return false;
         if (left.y < right.y) return true;
         if (left.y > right.y) return false;
         return left.x < right.x;
         }
      else {
         if (left.board < right.board) return true;
         if (left.board > right.board) return false;
         if (left.row < right.row) return true;
         if (left.row > right.row) return false;
         return left.col < right.col;
         }
      }
   };



//std::sort(intervals.begin(), intervals.end(), intervalSorttype(0)); // sort by low
void CreateTopology() {
   vector<topologytype> topology_1board;
   int left_count = 0, right_count = 128;
   int r, c;
   for (r = 0; r < 44; r++) {
      for (c = 0; c < 3; c++) {
         bool direction = false;
         int x, y;
         if (r < 11) {
            direction = (r & 1);
            y = r;
            x = direction ? 2 - c : c;
            }
         else if (r < 22) {
            direction = (r & 1);
            y = 21 - r;
            x = 3 + (direction ? 2 - c : c);
            }
         else if (r < 33) {
            direction = (r & 1);
            y = r - 22;
            x = 6 + (direction ? 2 - c : c);
            }
         else {
            direction = (r & 1);
            y = 43 - r;
            x = 9 + (direction ? 2 - c : c);
            }
         topology_1board.push_back(topologytype());
         topology_1board.back().board = 0;
         topology_1board.back().row = r;
         topology_1board.back().col = c;
         topology_1board.back().x = x;
         topology_1board.back().y = y;
         if (!direction)
            topology_1board.back().id = left_count++;
         else
            topology_1board.back().id = right_count++;
         }
      }
   
      std::sort(topology_1board.begin(), topology_1board.end(), topologySorttype(1)); // sort by y, x
      for (int i = 0; i < topology_1board.size(); i++)
         {
         const topologytype& t = topology_1board[i];
         printf("[%2d/%d id:%3d] ", t.row, t.col, t.id);
         if (i + 1 == topology_1board.size() || t.y != topology_1board[i + 1].y) printf("\n");
         }
      std::sort(topology_1board.begin(), topology_1board.end(), topologySorttype(0)); // sort by row, col
      printf("\ntopologytype topology[%d] = {",topology_1board.size());
      for (int i = 0; i < topology_1board.size(); i++)
         {
         const topologytype& t = topology_1board[i];
         printf("topologytype(%d,%d,%d),", t.row, t.col, t.id);
         }
      printf("};\n");
   
   }

/*
void Simulator()
   {
   float variation=0.0, ambient=25.0;
   vector<topologytype> topology_1board{ topologytype(0, 0, 0), topologytype(0, 1, 1), topologytype(0, 2, 2), topologytype(1, 0, 128), topologytype(1, 1, 129), topologytype(1, 2, 130), topologytype(2, 0, 3), topologytype(2, 1, 4), topologytype(2, 2, 5), topologytype(3, 0, 131), topologytype(3, 1, 132), topologytype(3, 2, 133), topologytype(4, 0, 6), topologytype(4, 1, 7), topologytype(4, 2, 8), topologytype(5, 0, 134), topologytype(5, 1, 135), topologytype(5, 2, 136), topologytype(6, 0, 9), topologytype(6, 1, 10), topologytype(6, 2, 11), topologytype(7, 0, 137), topologytype(7, 1, 138), topologytype(7, 2, 139), topologytype(8, 0, 12), topologytype(8, 1, 13), topologytype(8, 2, 14), topologytype(9, 0, 140), topologytype(9, 1, 141), topologytype(9, 2, 142), topologytype(10, 0, 15), topologytype(10, 1, 16), topologytype(10, 2, 17), topologytype(11, 0, 143), topologytype(11, 1, 144), topologytype(11, 2, 145), topologytype(12, 0, 18), topologytype(12, 1, 19), topologytype(12, 2, 20), topologytype(13, 0, 146), topologytype(13, 1, 147), topologytype(13, 2, 148), topologytype(14, 0, 21), topologytype(14, 1, 22), topologytype(14, 2, 23), topologytype(15, 0, 149), topologytype(15, 1, 150), topologytype(15, 2, 151), topologytype(16, 0, 24), topologytype(16, 1, 25), topologytype(16, 2, 26), topologytype(17, 0, 152), topologytype(17, 1, 153), topologytype(17, 2, 154), topologytype(18, 0, 27), topologytype(18, 1, 28), topologytype(18, 2, 29), topologytype(19, 0, 155), topologytype(19, 1, 156), topologytype(19, 2, 157), topologytype(20, 0, 30), topologytype(20, 1, 31), topologytype(20, 2, 32), topologytype(21, 0, 158), topologytype(21, 1, 159), topologytype(21, 2, 160), topologytype(22, 0, 33), topologytype(22, 1, 34), topologytype(22, 2, 35), topologytype(23, 0, 161), topologytype(23, 1, 162), topologytype(23, 2, 163), topologytype(24, 0, 36), topologytype(24, 1, 37), topologytype(24, 2, 38), topologytype(25, 0, 164), topologytype(25, 1, 165), topologytype(25, 2, 166), topologytype(26, 0, 39), topologytype(26, 1, 40), topologytype(26, 2, 41), topologytype(27, 0, 167), topologytype(27, 1, 168), topologytype(27, 2, 169), topologytype(28, 0, 42), topologytype(28, 1, 43), topologytype(28, 2, 44), topologytype(29, 0, 170), topologytype(29, 1, 171), topologytype(29, 2, 172), topologytype(30, 0, 45), topologytype(30, 1, 46), topologytype(30, 2, 47), topologytype(31, 0, 173), topologytype(31, 1, 174), topologytype(31, 2, 175), topologytype(32, 0, 48), topologytype(32, 1, 49), topologytype(32, 2, 50), topologytype(33, 0, 176), topologytype(33, 1, 177), topologytype(33, 2, 178), topologytype(34, 0, 51), topologytype(34, 1, 52), topologytype(34, 2, 53), topologytype(35, 0, 179), topologytype(35, 1, 180), topologytype(35, 2, 181), topologytype(36, 0, 54), topologytype(36, 1, 55), topologytype(36, 2, 56), topologytype(37, 0, 182), topologytype(37, 1, 183), topologytype(37, 2, 184), topologytype(38, 0, 57), topologytype(38, 1, 58), topologytype(38, 2, 59), topologytype(39, 0, 185), topologytype(39, 1, 186), topologytype(39, 2, 187), topologytype(40, 0, 60), topologytype(40, 1, 61), topologytype(40, 2, 62), topologytype(41, 0, 188), topologytype(41, 1, 189), topologytype(41, 2, 190), topologytype(42, 0, 63), topologytype(42, 1, 64), topologytype(42, 2, 65), topologytype(43, 0, 191), topologytype(43, 1, 192), topologytype(43, 2, 193) };
   serialtype serial;
   char buffer[256];


   printf("Please enter variation[0-3.0] ");
   gets_s(buffer, sizeof(buffer));
   sscanf(buffer, "%f", &variation);
   printf("Please enter ambient inlet temp ");
   gets_s(buffer, sizeof(buffer));
   sscanf(buffer, "%f", &ambient);
   
//   variation = 1.0; ambient = 45.0;


   scenariotype scenario(systeminfotype(), 44, 3, "shmoo/bin1smt_a.csv");

//   serial.Init("\\\\.\\COM17");
   serial.Init("COM8");
   serial.SetBaudRate(115200);
//   serial.SetBaudRate(500000);
   serial.DrainReceiver();
   while (true) {
      command_cfgtype cmdpacket;
      char magic[4], in;

      if (1 == serial.RcvData(&in, 1, true))
         {
         magic[3] = magic[2];
         magic[2] = magic[1];
         magic[1] = magic[0];
         magic[0] = in;
         }

      cmdpacket.command_unique = (magic[3] << 0) | (magic[2] << 8) | (magic[1] << 16) | (magic[0] << 24);

      if (cmdpacket.command_unique== CMD_UNIQUE && sizeof(cmdpacket) - 4 == serial.RcvData((char*)&cmdpacket + 4, sizeof(cmdpacket) - 4, true))
         {
         magic[0] = 0;
         if ((cmdpacket.command & 0xf) >= 4) {
            command_loadtype loadpacket;
            memcpy(&loadpacket, &cmdpacket, sizeof(cmdpacket));
            serial.RcvData((char*)&loadpacket + sizeof(cmdpacket), sizeof(command_loadtype) - sizeof(cmdpacket), true);
            }
         else if (!cmdpacket.IsCrcCorrect()) {
            printf("CRC ERROR\n");
            }
         else if (cmdpacket.command){
            // I have a correctly formed config packet
            response_cfgtype rsppacket;
            printf("I saw a packet to id:%d address:%d\n", cmdpacket.id, cmdpacket.address);
            scenario.Program(cmdpacket);
            if ((cmdpacket.command & 0xf) == CMD_READ && !scenario.Respond(cmdpacket, rsppacket.data)) {
               rsppacket.response_unique = RSP_UNIQUE;
               rsppacket.command = cmdpacket.command;
               rsppacket.address = cmdpacket.address;
               rsppacket.id = cmdpacket.id;
               rsppacket.GenerateCrc();
               serial.SendData(&rsppacket, sizeof(rsppacket));
               }
            if (cmdpacket.command == CMD_WRITE && cmdpacket.address == E_PLLFREQ)
               {
               float power, hashrate, hitrate, max_temp, avg_temp, outlet_temp, ir, efficiency;
               scenario.Stats(power, hashrate, hitrate, max_temp, avg_temp, outlet_temp, ir, efficiency, true);
//               printf("%.2fV, Tj-max = %5.1fC Tj delta =%4.1fC, V delta =%4.1fmV, Outlet=%.1fC, theoretical hashrate = %.1fTH/s, hashrate = %.1fTH/s, Power = %.0fW, Eff = %.2fJ/TH    \r", scenario.supply, maxj, maxj - minj, deltav * 1000.0, outlet, possible, hashrate, power, power / hashrate);
               }
            }
         }
      }
   }
*/



