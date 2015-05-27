/*************************************************************************
Title:    MRBusClock Arduino Library Header
Authors:  Nathan Holmes <maverick@drgw.net>, Colorado, USA
          Michael Petersen <railfan@drgw.net>, Colorado, USA
File:     MRBusClock.h
License:  GNU General Public License v3

LICENSE:
    Copyright (C) 2015 Nathan Holmes, Michael Petersen
    
    The MRBusClock library provides a way to easily interface Arduino applications
    with MRBus-based clock systems (such as the Iowa Scaled Engineering
    MRB-FCM, etc.)
    
    The latest source can be obtained from ISE's Github repository here:
    https://github.com/IowaScaledEngineering/MRBusClock

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License along
    with this program. If not, see http://www.gnu.org/licenses/
    
*************************************************************************/

#ifndef _MRBUSCLOCK_H_
#define _MRBUSCLOCK_H_

#include "MRBusArduino.h"

class MRBusTime
{
	public:
		MRBusTime();
		MRBusTime(uint8_t hours, uint8_t minutes, uint8_t seconds=0);
		void init();
		void safe_copy(const MRBusTime & intime);
		void addSeconds(uint8_t seconds);
		bool setTime(uint8_t hours, uint8_t minutes, uint8_t seconds);
		bool operator<= (const MRBusTime& rhv);
		bool operator< (const MRBusTime& rhv);
		bool operator>= (const MRBusTime& rhv);
		bool operator> (const MRBusTime& rhv);
		bool operator== (const MRBusTime& rhv);
		bool operator!= (const MRBusTime& rhv);
		MRBusTime& operator= (const MRBusTime& rhv);
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
		void begin();
		bool processTimePacket(const MRBusPacket & pkt);
		void setTimeout(uint8_t decisecs);
		void setTimeSourceAddress(uint8_t addr);		
		uint8_t getTimeSourceAddress();
		bool isTimedOut();
		bool isOnFastTime();
		bool isOnRealTime();
		bool isOnHold();	
		bool isRealIn12HDisplay();
		bool isFastIn12HDisplay();
		bool getFastTime(MRBusTime & time);
		bool getRealTime(MRBusTime & time);
		
		
	private:
		void initVars();
		uint8_t timeSourceAddress;
		uint8_t maxDecisecsTimeout;
};


#endif

