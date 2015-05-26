#include "MRBusClock.h"

#ifdef __AVR__
#include "util/atomic.h"
#else
#error "This library only supports boards with an AVR processor."
#endif

//*****************************************************************************
// MRBusTime Class
// Holds representation of hours/minutes/seconds time
//*****************************************************************************

MRBusTime::MRBusTime()
{
	this->init();
}

void MRBusTime::init()
{
	this->seconds = this->minutes = this->hours = 0;
}

void MRBusTime::safe_copy(const MRBusTime & intime)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		this->hours = intime.hours;
		this->minutes = intime.minutes;
		this->seconds = intime.seconds;
	}
}

void MRBusTime::addSeconds(uint8_t seconds)
{
	uint16_t i = this->seconds + seconds;

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

MRBusTime& MRBusTime::operator= (const MRBusTime& rhv) 
{
	this->hours = rhv.hours;
	this->minutes = rhv.minutes;
	this->seconds = rhv.seconds;	
	return *this;
}

bool MRBusTime::operator<(const MRBusTime& rhv)
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

bool MRBusTime::operator<=(const MRBusTime& rhv)
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


bool MRBusTime::operator>(const MRBusTime& rhv)
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

bool MRBusTime::operator>=(const MRBusTime& rhv)
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


bool MRBusTime::operator==(const MRBusTime& rhv)
{
	return (this->hours == rhv.hours && this->minutes == rhv.minutes && this->seconds == rhv.seconds);
}

bool MRBusTime::operator!=(const MRBusTime& rhv)
{
	return !(this->hours == rhv.hours && this->minutes == rhv.minutes && this->seconds == rhv.seconds);
}

bool MRBusTime::setTime(uint8_t hours, uint8_t minutes, uint8_t seconds=0)
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

//*****************************************************************************
// End of MRBusTime Class
//*****************************************************************************

//*****************************************************************************
// Global (restricted to this file) variables needed to interact with timer interrupt
//*****************************************************************************


static volatile uint8_t timePacketTimeout;
static volatile uint16_t fastTimeDecisecs; 
static volatile uint8_t scaleTenthsAccum;
static uint16_t scaleFactor;
static uint8_t mrbusTimeFlags;

static MRBusTime realTime;
static MRBusTime fastTime;

//*****************************************************************************
// End Globals
//*****************************************************************************

#define TICKS_PER_DECISEC 11

void initializeTickTimer(void)
{
	// Set up timer 1 for 110Hz interrupts at 16MHz Arduino Clock
	// This actually gives a 0.032% error.  However, I didn't want to use timer 1 since it gets used by
	// other libraries.
	TCNT2 = 0;
	OCR2A = 0x8D;
	TCCR2A = _BV(WGM21);
	TCCR2B = _BV(CS22) | _BV(CS21) | _BV(CS20);
	TIMSK2 |= _BV(OCIE2A);
}


ISR(TIMER2_COMPA_vect)
{
	static uint8_t ticks = 0;

	// Since it's a multiple of 10Hz, cut it down by a constant associated with the timer
	ticks++;
	if (ticks < TICKS_PER_DECISEC)
		return;
	ticks -= TICKS_PER_DECISEC;
	
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
		uint8_t fastTimeSecs = fastTimeDecisecs / 10;
		fastTimeDecisecs -= fastTimeSecs * 10;	
		fastTime.addSeconds(fastTimeSecs);
	}
}

#undef TICKS_PER_DECISEC

//*****************************************************************************
// MRBusClock Class
// Used to handle interactions with a MRBus-based fast clock
//*****************************************************************************

MRBusClock::MRBusClock()
{
	this->maxDecisecsTimeout = 50;
	this->timeSourceAddress = 0xFF;
	timePacketTimeout = 0;
	fastTimeDecisecs = 0;
	scaleTenthsAccum = 0;
	realTime.init();
	fastTime.init();	
	return;
}

bool MRBusClock::isOnFastTime()
{
	if (TIME_FLAGS_DISP_FAST == (mrbusTimeFlags & (TIME_FLAGS_DISP_FAST | TIME_FLAGS_DISP_FAST_HOLD)))
		return true;
	
	return false;
}

bool MRBusClock::isOnHold()
{
	if ((TIME_FLAGS_DISP_FAST | TIME_FLAGS_DISP_FAST_HOLD) == (mrbusTimeFlags & (TIME_FLAGS_DISP_FAST | TIME_FLAGS_DISP_FAST_HOLD)))
		return true;
	
	return false;
}

bool MRBusClock::isOnRealTime()
{
	if (0 == (mrbusTimeFlags & (TIME_FLAGS_DISP_FAST | TIME_FLAGS_DISP_FAST_HOLD)))
		return true;
	
	return false;
}

bool MRBusClock::isTimedOut()
{
	if ((0 == maxDecisecsTimeout) || timePacketTimeout)
		return false;
	
	return true;
}

bool MRBusClock::getFastTime(MRBusTime & time)
{
	if (this->isTimedOut())
	{
		time.init();
		return false;
	}
	
	time.safe_copy(fastTime);
	return true;
}

bool MRBusClock::isFastIn12HDisplay()
{
	return (mrbusTimeFlags & TIME_FLAGS_DISP_FAST_AMPM)?true:false;
}

bool MRBusClock::isRealIn12HDisplay()
{
	return (mrbusTimeFlags & TIME_FLAGS_DISP_REAL_AMPM)?true:false;
}

bool MRBusClock::getRealTime(MRBusTime & time)
{
	if (this->isTimedOut())
	{
		time.init();
		return false;
	}
	
	time = realTime;
	return true;
}


void MRBusClock::setTimeSourceAddress(uint8_t srcAddress)
{
	this->timeSourceAddress = srcAddress;
}

uint8_t MRBusClock::getTimeSourceAddress()
{
	return (this->timeSourceAddress);
}

void MRBusClock::setTimeout(uint8_t decisecs)
{
	this->maxDecisecsTimeout = decisecs;
}


bool MRBusClock::processTimePacket(const MRBusPacket & rxpkt)
{
	// Verify that either we're listening to all time sources, or it's a packet from our
	// specified time source
	if ((0xFF != this->timeSourceAddress) && 
		(rxpkt.pkt[MRBUS_PKT_SRC] != this->timeSourceAddress))
		return false;

	// Just straight up update real time
	// Real time doesn't move fast enough to need dead reckoning between packets
	realTime.hours = rxpkt.pkt[6];
	realTime.minutes = rxpkt.pkt[7];
	realTime.seconds = rxpkt.pkt[8];
	mrbusTimeFlags = rxpkt.pkt[9];
	
	// Time source packets aren't required to have a fast section
	// Doesn't really make sense outside model railroading applications, so handle the case that
	// there's no fast time section
	if (rxpkt.pkt[MRBUS_PKT_LEN] >= 14)
	{
		fastTime.hours =  rxpkt.pkt[10];
		fastTime.minutes =  rxpkt.pkt[11];
		fastTime.seconds = rxpkt.pkt[12];
		scaleFactor = (((uint16_t)rxpkt.pkt[13])<<8) + (uint16_t)rxpkt.pkt[14];
	}		
	// If we got a packet, there's no dead reckoning time anymore
	fastTimeDecisecs = 0;
	scaleTenthsAccum = 0;
	timePacketTimeout = this->maxDecisecsTimeout;
	return true;
}


//*****************************************************************************
// End of MRBusClock Class
//*****************************************************************************



