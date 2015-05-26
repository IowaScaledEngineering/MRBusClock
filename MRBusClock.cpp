#include "MRBusTime.h"

MRBusTimeData::MRBusTimeData()
{
	this->seconds = this->minutes = this->hours = 0;
}

void MRBusTimeData::init()
{
	this->seconds = this->minutes = this->hours = 0;
}

void MRBusTimeData::copy(const MRBusTimeData & intime)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		this->hours = intime.hours;
		this->minutes = intime.minutes;
		this->seconds = intime.seconds;
	}
}

void MRBusTimeData::increment(uint8_t seconds)
{
	uint16_t i = this->seconds + incSeconds;

	while(i >= 60)
	{
		this->minutes++;
		i -= 60;
	}
	this->seconds = (uint8_t)i;
	
	while(this->minutes >= 60)
	{
		this->hours++;
		this->minutes -= 60;
	}
	
	if (this->hours >= 24)
		this->hours %= 24;
}

MRBusTimeData& MRBusTimeData::operator= (const MRBusTimeData& rhv) 
{
	this->hours = rhv.hours;
	this->minutes = rhv.minutes;
	this->seconds = rhv.seconds;	
	return *this;
}

bool MRBusTimeData::operator <(const MRBusTimeData& rhv)
{
	if (this->hours == rhv.hours)
	{
		if (this->minutes == rhv.minutes)
			return (this->seconds < rhv.seconds);
		else
			return (this->minutes < rhv.minutes);	
	}
	else
		return (this->hours < rhv.hours);
}

bool MRBusTimeData::operator <=(const MRBusTimeData& rhv)
{
	if (this->hours == rhv.hours && this->minutes == rhv.minutes && this->seconds == rhv.seconds)
		return true;

	if (this->hours == rhv.hours)
	{
		if (this->minutes == rhv.minutes)
			return (this->seconds < rhv.seconds);
		else
			return (this->minutes < rhv.minutes);	
	}
	else
		return (this->hours < rhv.hours);
}


bool MRBusTimeData::operator >(const MRBusTimeData& rhv)
{
	if (this->hours == rhv.hours)
	{
		if (this->minutes == rhv.minutes)
			return (this->seconds > rhv.seconds);
		else
			return (this->minutes > rhv.minutes);	
	}
	else
		return (this->hours > rhv.hours);
}

bool MRBusTimeData::operator >=(const MRBusTimeData& rhv)
{
	if (this->hours == rhv.hours && this->minutes == rhv.minutes && this->seconds == rhv.seconds)
		return true;

	if (this->hours == rhv.hours)
	{
		if (this->minutes == rhv.minutes)
			return (this->seconds > rhv.seconds);
		else
			return (this->minutes > rhv.minutes);	
	}
	else
		return (this->hours > rhv.hours);
}


bool MRBusTimeData::operator ==(const MRBusTimeData& rhv)
{
	return (this->hours == rhv.hours && this->minutes == rhv.minutes && this->seconds == rhv.seconds);
}

bool MRBusTimeData::operator !=(const MRBusTimeData& rhv)
{
	return !(this->hours == rhv.hours && this->minutes == rhv.minutes && this->seconds == rhv.seconds);
}

bool MRBusTimeData::setTime(uint8_t hours, uint8_t minutes, uint8_t seconds=0)
{
	if (hours >= 24 || minutes >= 60 || seconds >= 60)
	{
		this->init();
		return false;
	}
	
	this->hours = hours;
	this->minutes = minutes;
	this->seconds = seconds;
	return true;
}


static volatile uint8_t timePacketTimeout;
static volatile uint16_t fastTimeDecisecs; 
static volatile uint8_t scaleTenthsAccum;
static volatile uint16_t scaleFactor;
static volatile uint8_t mrbusTimeFlags;

static MRBusTimeData realTime;
static MRBusTimeData fastTime;


ISR(TIMER0_COMPA_vect)
{
	if (timePacketTimeout)
		timePacketTimeout--;
	
	if ( TIME_FLAGS_DISP_FAST == (mrbusTimeFlags & (TIME_FLAGS_DISP_FAST | TIME_FLAGS_DISP_FAST_HOLD)) )
		return; // If we're not running fast time, bail, no need for all this junk
	
	fastTimeDecisecs += scaleFactor / 10;
	scaleTenthsAccum += scaleFactor % 10;
	if (scaleTenthsAccum > 10)
	{
		fastTimeDecisecs++;
		scaleTenthsAccum -= 10;
	}
	
	if (fastTimeDecisecs >= 10)
	{
		uint8_t fastTimeSecs = fastDecisecs / 10
		fastDecisecs -= fastTimeSecs * 10;	
		fastTime.increment(fastTimeSecs);
	}
}

bool MRBusTime::isOnFastTime()
{
	if (TIME_FLAGS_DISP_FAST == (mrbusTimeFlags & (TIME_FLAGS_DISP_FAST | TIME_FLAGS_DISP_FAST_HOLD))
		return true;
	
	return false;
}

bool MRBusTime::isOnHold()
{
	if ((TIME_FLAGS_DISP_FAST | TIME_FLAGS_DISP_FAST_HOLD) == (mrbusTimeFlags & (TIME_FLAGS_DISP_FAST | TIME_FLAGS_DISP_FAST_HOLD))
		return true;
	
	return false;
}

bool MRBusTime::isOnRealTime()
{
	if (0 == (mrbusTimeFlags & (TIME_FLAGS_DISP_FAST | TIME_FLAGS_DISP_FAST_HOLD))
		return true;
	
	return false;
}

bool MRBusTime::isTimedOut()
{
	if (timePacketTimeout)
		return false;
	
	return true;
}

bool MRBusTime::getFastTime(MRBusTimeData & time)
{
	if (this->isTimedOut())
	{
		time.init();
		return false;
	}
	
	time.copy(fastTime);
	return true;
}

bool MRBusTime::getRealTime(MRBusTimeData & time)
{
	if (this->isTimedOut())
	{
		time.init();
		return false;
	}
	
	time.copy(realTime);
	return true;
}


void MRBusTime::setTimeSourceAddress(uint8_t srcAddress)
{
	this->timeSourceAddress = srcAddress;
}

uint8_t MRBusTime::getTimeSourceAddress()
{
	return (this->timeSourceAddress);
}

bool MRBusTime::processTimePacket(const MRBusPacket & rxpkt)
{
	// Verify that either we're listening to all time sources, or it's a packet from our
	// specified time source
	if ((0xFF != this->timeSourceAddress) && 
		(rxpkt.pkt[MRBUS_PKT_SRC] != timeSourceAddress)) )
		return;

	// Just straight up update real time
	// Real time doesn't move fast enough to need dead reckoning between packets
	this->realTime.hours = rxpkt.pkt[6];
	this->realTime.minutes = rxpkt.pkt[7];
	this->realTime.seconds = rxpkt.pkt[8];
	flags = mrbus_rx_buffer[9];
	
	// Time source packets aren't required to have a fast section
	// Doesn't really make sense outside model railroading applications, so handle the case that
	// there's no fast time section
	if (rxpkt.pkt[MRBUS_PKT_LEN] >= 14)
	{
		this->fastTime.hours =  rxpkt.pkt[10];
		this->fastTime.minutes =  rxpkt.pkt[11];
		this->fastTime.seconds = rxpkt.pkt[12];
		this->scaleFactor = (((uint16_t)rxpkt.pkt[13])<<8) + (uint16_t)rxpkt.pkt[14];
	}		
	// If we got a packet, there's no dead reckoning time anymore
	this->fastTimeDecisecs = 0;
	this->scaleTenthsAccum = 0;
	this->timePacketTimeout = this->maxDecisecsTimeout;
	return;

}

void MRBusTime::setTimeout(uint8_t decisecs)
{
	this->maxDecisecsTimeout = max(1, decisecs);
}

void MRBusTime::MRBusTime()
{
	this->maxDecisecsTimeout = 50;
	this->timeSourceAddress = 0xFF;
	this->timePacketTimeout = 0;
	this->fastTimeDecisecs = 0;
	this->scaleTenthsAccum = 0;
	return;
}


