#ifndef _MRBUSCLOCK_H_
#define _MRBUSCLOCK_H_

#include "mrbus-arduino.h"

class MRBusTime
{
	public:
		MRBusTime();
		bool setTime(uint8_t hours, uint8_t minutes, uint8_t seconds);
		bool operator<= (const MRBusTimeData& rhv);
		bool operator< (const MRBusTimeData& rhv);
		bool operator>= (const MRBusTimeData& rhv);
		bool operator> (const MRBusTimeData& rhv);
		bool operator== (const MRBusTimeData& rhv);
		bool operator!= (const MRBusTimeData& rhv);
		MRBusTimeData& operator= (const MRBusTimeData& rhv);
		uint8_t seconds;
		uint8_t minutes;
		uint8_t hours;
};

#define TIME_FLAGS_DISP_FAST       0x01
#define TIME_FLAGS_DISP_FAST_HOLD  0x02
#define TIME_FLAGS_DISP_REAL_AMPM  0x04
#define TIME_FLAGS_DISP_FAST_AMPM  0x08

class MRBusClock
{
	public:
		MRBusClock();
		bool processTimePacket(const MRBusPacket & pkt);
		void incrementTime(MRBusWhichTime whichTime, uint8_t incSeconds);
		void setTimeout(uint8_t decisecs);
		void setTimeSourceAddress(uint8_t addr);		
		uint8_t getTimeSourceAddress();
		bool isTimedOut();
		bool isOnFastTime();
		bool isOnRealTime();
		bool isOnHold();		
		bool getFastTime(MRBusTimeData & time);
		bool getRealTime(MRBusTimeData & time);
		
		
	private:
		uint8_t timeSourceAddress;
		uint8_t maxDecisecsTimeout;
};


#endif

