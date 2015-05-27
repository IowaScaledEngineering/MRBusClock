/*************************************************************************
Title:    MRBusClock Arduino Library Example Sketch
Authors:  Nathan Holmes <maverick@drgw.net>, Colorado, USA
          Michael Petersen <railfan@drgw.net>, Colorado, USA
File:     MRBusClock.h
License:  GNU General Public License v3

LICENSE:
    Copyright (C) 2015 Nathan Holmes, Michael Petersen
    
    This example sketch provides an easy way to test out the MRBusClock library.
    It also needs the MRBus for Arduino library, which can be found in the src/
    directory of the Iowa Scaled Engineering mrb-ard project:
    https://github.com/IowaScaledEngineering/mrb-ard
    
    The best way to test this is using an Arduino Leonardo or similar, since it
    can use the USB connection for printing while not interfering with the MRBus
    interface.  While using an ISE MRB-ARD MRBus Arduino shield is the easiest
    way to get everything up and running, all it really needs is an RS485 driver
    hooked up appropriately.
    
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

#include <MRBusArduino.h>
#include <MRBusClock.h>
#include <EEPROM.h>

MRBus mrbus;
MRBusClock mrbusclock;

unsigned long lastUpdateTime, lastPrintTime, lastTransmitTime, currentTime;
unsigned long updateInterval = 100, printInterval = 500;

void setup()
{
  uint8_t address = EEPROM.read(MRBUS_EE_DEVICE_ADDR);
  Serial.begin(9600);
  while(!Serial); // Waits for Leonardos to get the USB interface up and running

  mrbus.begin();
  mrbusclock.begin();
  
  if (0xFF == address)
  {
    EEPROM.write(MRBUS_EE_DEVICE_ADDR, 0x03);
    address = EEPROM.read(MRBUS_EE_DEVICE_ADDR);
  }

  mrbus.setNodeAddress(address);
  lastPrintTime = lastUpdateTime = currentTime = millis();
  
  // Set 10 second (100 decisec) timeout on our clock object
  mrbusclock.setTimeout(100); 
  // Set the time source to the broadcast address so we'll listen to anything
  mrbusclock.setTimeSourceAddress(0xFF);
  
  // Make the onboard LED an output
  pinMode(13, OUTPUT);
}

void pktActions(MRBusPacket& mrbPkt)
{
  // Is it broadcast or for us?  Otherwise ignore
  if (0xFF != mrbPkt.pkt[MRBUS_PKT_DEST] && mrbus.getNodeAddress() != mrbPkt.pkt[MRBUS_PKT_DEST])
    return;

  if (false == mrbus.doCommonPacketHandlers(mrbPkt))
  {
    if ('T' == mrbPkt.pkt[MRBUS_PKT_TYPE])
       mrbusclock.processTimePacket(mrbPkt);
  }
}

void printMRBusTime(MRBusTime time)
{
  char buffer[12];
  memset(buffer, 0, sizeof(buffer));
  sprintf(buffer, "%02d:%02d:%02d", time.hours, time.minutes, time.seconds);
  Serial.print(buffer);
}


void loop()
{
  currentTime = millis();
  updateInterval = 100;
  
  // Really, seriously, no point in checking more than every 100mS
  if (currentTime - lastUpdateTime >= updateInterval)
  {
    // isOnFastTime() checks that the clock is running in fast time mode
    // isTimedOut() checks if we've timed out since we've received our last clock communication - default is 5 seconds
    if (mrbusclock.isOnFastTime() && !mrbusclock.isTimedOut())
    {
      // Retrieve the current fast time
      MRBusTime currentFastTime;
      mrbusclock.getFastTime(currentFastTime);

      // Do something useful with the time - if the fast clock is between 0300h and 0400h, turn on the light on pin 13  
      // Actual applications would likely do something much more interesting
      if (currentFastTime > MRBusTime(3, 0) && currentFastTime < MRBusTime(4, 0))
        digitalWrite(13, HIGH);
      else
        digitalWrite(13, LOW);
    }
    else
    {
       // This is the case where we're either on real time, or we timed out
       // Either way, shut the light off
       digitalWrite(13, LOW);
    }
    
    lastUpdateTime = currentTime;
  }

  // Only print the time every 0.5s or so
  // This only works if you're on a Leonardo - if you're on an Uno or other 328-based Arduino,
  // this will step on MRBus.  That's what the if defined block is here to do
#if defined(__AVR_ATmega32U4__)
  if (currentTime - lastPrintTime >= printInterval)
  {
    lastPrintTime = currentTime;
    Serial.print("The time is...");
    
    if (mrbusclock.isTimedOut())
    {
      Serial.print("Timed out");
    }
    else if (mrbusclock.isOnFastTime()) 
    {
      MRBusTime currentFastTime;
      mrbusclock.getFastTime(currentFastTime);      
      Serial.print("FAST [");
      printMRBusTime(currentFastTime);      
      Serial.print("]");
    }
    else if (mrbusclock.isOnHold()) 
    {
      Serial.print("ON HOLD");
    }
    else if (mrbusclock.isOnRealTime()) 
    {
      MRBusTime currentRealTime;
      
      mrbusclock.getRealTime(currentRealTime);      
      Serial.print("REAL [");
      printMRBusTime(currentRealTime);      
      Serial.print("]");
    } else {
      Serial.print(" mysteriously broken...");
    }
    Serial.print("\n");
  }
#endif
  
  // If we have packets, try parsing them
  if (mrbus.hasRxPackets())
  {
    MRBusPacket mrbPkt;
    mrbus.getReceivedPacket(mrbPkt);
    pktActions(mrbPkt);
  }

  // If there are packets to transmit and it's been more than 20mS since our last transmission attempt, try again
  if (mrbus.hasTxPackets() && ((lastTransmitTime - currentTime) > 20))
  {
     lastTransmitTime = currentTime;
     mrbus.transmit();
  }
}

