/****************************************************************************

  KOM RSVP Engine (release version 3.0f)
  Copyright (C) 1999-2004 Martin Karsten

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  Contact:	Martin Karsten
		TU Darmstadt, FG KOM
		Merckstr. 25
		64283 Darmstadt
		Germany
		Martin.Karsten@KOM.tu-darmstadt.de

  Other copyrights might apply to parts of this package and are so
  noted when applicable. Please see file COPYRIGHT.other for details.

****************************************************************************/
#ifndef _RSVP_BaseTimer_h_
#define _RSVP_BaseTimer_h_ 1

#include "RSVP_Global.h"
#include "RSVP_Log.h"
#include "RSVP_TimeValue.h"
#include "RSVP_SortedList.h"

class BaseTimer;
struct Less<BaseTimer*> {
	inline bool operator()( BaseTimer*, BaseTimer* ) const;
};
typedef SortableList<BaseTimer*,BaseTimer*> BaseTimerList;

namespace TG { class TrafficGenerator; }

class TimerSystem {
	// static members are defined in RSVP_Global.cc
	static sint32 slotCount;
	static TimeValue totalPeriod;                          // in seconds
	static TimeValue slotLength;
	static TimeValue timerResolution;
	static sint32 maxDeltaSlots;
	sint32 currentSlot;
	TimeValue currentTime;
	TimeValue epochBaseTime;
	BaseTimerList* currentFireList;
	BaseTimerList* timerList;
	BaseTimerList* endOfList;
	BaseTimerList futureTimerList;
	inline void fire();
	inline sint32 getSlotNumber( const TimeValue& t );
	inline BaseTimer* getNextAlarm();
	bool endFlag;
	friend class ConfigFileReader;                      // access: totalPeriod, slotCount, slotLength
	friend class TG::TrafficGenerator;                  // access: totalPeriod, slotCount, slotLength
public:
	void start();
	TimeValue checkDrift();
	static const sint32 K = 3;                       // factor for timeout
	static const sint32 Kb = 3;                      // factor for blockade timeout
	// defined in RSVP_Global.cc
	static const TimeValue W;                        // wait time before local repair
	TimerSystem();
	~TimerSystem();
	void reportSettings();
	BaseTimerList::ConstIterator insertTimer( BaseTimer* );
	void eraseTimer( BaseTimerList::ConstIterator );
	bool executeTimer( TimeValue& );
#if defined(NS2)
	const TimeValue& getCurrentTime() { currentTime = getCurrentSystemTime(); return currentTime; }
#else
	const TimeValue& getCurrentTime() { return currentTime; }
#endif
};

class BaseTimer {
protected:
	TimeValue alarmTime;                              // absolut time value
	BaseTimerList::ConstIterator myIterator;
	friend ostream& operator<< ( ostream&, const BaseTimer& );
	friend class TimerSystem;                         // access: reschedule
	void start() {
                                                         assert( !myIterator );
		myIterator = RSVP_Global::currentTimerSystem->insertTimer( this );
	}
	void reschedule() {
                                                          assert( myIterator );
		myIterator.reset();
		start();
	}
public:
	virtual void internalFire() = 0;
public:
	BaseTimer( const TimeValue& timeout ) {
		if ( timeout != TimeValue(0,0) ) {
			alarmTime = timeout + getCurrentSystemTime();
			start();
		}
	}
	BaseTimer( const BaseTimer& b ) : alarmTime(b.alarmTime) {}
	virtual ~BaseTimer() {
		LOG(4)( Log::Timer, "timer", this, *this, "deleted" );
		cancel();
	}
	void cancel() {
		if ( myIterator ) {
			RSVP_Global::currentTimerSystem->eraseTimer( myIterator );
			myIterator.reset();
		}
	}
	void restart( const TimeValue& timeout ) {
		if ( timeout != TimeValue(0,0) ) {
			cancel();
			alarmTime = timeout + RSVP_Global::currentTimerSystem->getCurrentTime();
			start();
		}
	}
	bool isActive() const { return myIterator; }
	const TimeValue& getAlarmTime() const { return alarmTime; }
	TimeValue getRemainingTime() const {
		return alarmTime - getCurrentSystemTime();
	}
	DECLARE_ORDER(BaseTimer)
};

IMPLEMENT_ORDER1(BaseTimer,alarmTime)
inline bool Less<BaseTimer*>::operator()( BaseTimer* b1, BaseTimer* b2 ) const {
	return *b1 < *b2;
}

inline ostream& operator<< ( ostream& os, const BaseTimer& bt ) {
	os << (DaytimeTimeValue&)bt.alarmTime;
	return os;
}

// timeout is given in msec -> from TIME_VALUES_Object
extern inline TimeValue multiplyTimeoutTime( sint32 timeout ) {
#if defined(FIXED_TIMEOUTS)
	timeout = timeout * TimerSystem::K;
#else
	timeout = ( timeout * TimerSystem::K * 3 ) / 2;
#endif
	return TimeValue( timeout/MSECS_PER_SEC, (timeout % MSECS_PER_SEC)*USECS_PER_MSEC );
}

extern inline TimeValue multiplyTimeoutTime( TimeValue timeout ) {
#if defined(FIXED_TIMEOUTS)
	timeout = timeout * TimerSystem::K;
#else
	timeout = ( timeout * TimerSystem::K * 3 ) / 2;
#endif
	return timeout;
}

extern inline TimeValue randomizeRefreshTime( const TimeValue& refresh ) {
#if defined(FIXED_TIMEOUTS)
	return refresh;
#else
	return ( refresh * (500+drawRandomNumber(1000)) ) / 1000;
#endif
}

extern inline TimeValue multiplyBlockadeTime( const TimeValue& timeout ) {
	return ( timeout * TimerSystem::Kb );
}

#endif /* _RSVP_BaseTimer_h_ */
