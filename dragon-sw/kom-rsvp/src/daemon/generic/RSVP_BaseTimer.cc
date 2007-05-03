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
#include "RSVP_BaseTimer.h"
#include "RSVP_Log.h"

TimerSystem::TimerSystem() : endFlag(false) {
#if defined(NO_TIMERS) || defined(NS2)
	slotCount = 1;
	slotLength = totalPeriod;
	timerResolution = TimeValue(0,20000);
#else
	if ( slotCount == 0 ) {
		timerResolution = measureTimerResolution();
	} else {
		timerResolution = totalPeriod / slotCount;
	}
	if ( slotLength == TimeValue(0,0) ) slotLength = timerResolution;
	if ( slotCount == 0 ) slotCount = totalPeriod / slotLength;
#endif
	if ( slotCount * slotLength != totalPeriod ) {
		cerr << "FATAL ERROR: slotLength * slotCount must equal totalPeriod." << endl;
		goto abort;
	}
#if defined(FUZZY_TIMERS)
	if ( slotLength > timerResolution*2 ) {
		cerr << "FATAL ERROR: slotLength " << slotLength << " is much larger than timerResolution " << timerResolution << endl;
		goto abort;
	}
#endif
#if defined(NS2)
	// avoid warnings during ns-2 simulation
	maxDeltaSlots = sint32Infinite;
#else
	maxDeltaSlots = TimeValue(1,0) / slotLength;
#endif
	timerList = new BaseTimerList[slotCount];
	endOfList = timerList + slotCount;
	getCurrentSystemTime(currentTime);
	epochBaseTime = (currentTime / totalPeriod) * totalPeriod;
	currentSlot = getSlotNumber( currentTime ) % slotCount;
	currentFireList = timerList + currentSlot;
	ERROR(7)( Log::Error, "Timer:", currentTime, currentSlot, totalPeriod, slotLength, slotCount, epochBaseTime );
	return;
abort:
	reportSettings();
	cerr << "Please correct these settings!!" << endl;
	abortProcess();
}

TimerSystem::~TimerSystem() {
	sint32 t = 0;
	sint32 i = 0;
	for ( ; i < slotCount; i += 1 ) {
		t += timerList[i].size();
		timerList[i].clear();
	}
	if ( t != 0 ) {
		cerr << "found " << t << " remaining timers during system cleanup" << endl;
	}
	delete [] timerList;
}

void TimerSystem::reportSettings() {
	cerr << "Current settings:" << endl
		<< "TIMER_SLOT_TOTAL_PERIOD = " << TIMER_SLOT_TOTAL_PERIOD << endl
		<< "TimerSystem::timerResolution = " << (PreciseTimeValue&)TimerSystem::timerResolution << endl
		<< "TimerSystem::slotLength = " << (PreciseTimeValue&)slotLength << endl
		<< "TimerSystem::slotCount = " << slotCount << endl
		<< "TimerSystem::totalPeriod = " << (PreciseTimeValue&)totalPeriod << " sec" << endl;
}

extern inline sint32 TimerSystem::getSlotNumber( const TimeValue& t ) {
                                                    assert(t >= epochBaseTime);
	return (t - epochBaseTime) / slotLength;
}

extern inline BaseTimer* TimerSystem::getNextAlarm() {
	return currentFireList->empty() ? (BaseTimer*)0 : currentFireList->front();
}

BaseTimerList::ConstIterator TimerSystem::insertTimer( BaseTimer* b ) {
#if defined(NO_TIMERS)
	return timerList[0].end();
#endif
	BaseTimerList::ConstIterator iter;
	sint32 timerSlot = getSlotNumber( b->getAlarmTime() );
                                              assert(timerSlot >= currentSlot);
	if ( (timerSlot - currentSlot) >= slotCount ) {
		iter = futureTimerList.insert_sorted( b );
	} else {
#if defined(FUZZY_TIMERS)
		iter = timerList[timerSlot%slotCount].push_back( b );
#else
		iter = timerList[timerSlot%slotCount].insert_sorted( b );
#endif
	}
	LOG(4)( Log::Timer, "timer", b, *b, "scheduled" );
	return iter;
}

void TimerSystem::eraseTimer( BaseTimerList::ConstIterator iter ) {
#if defined(NO_TIMERS)
	return;
#endif
	sint32 timerSlot = getSlotNumber( (*iter)->getAlarmTime() );
                                              assert(timerSlot >= currentSlot);
	if ( (timerSlot - currentSlot) >= slotCount ) {
		if ( futureTimerList.empty() ) {
			FATAL(4)( Log::Fatal, "attempt to remove from empty futureTimerList - currentSlot", currentSlot, "timerSlot", timerSlot );
			abortProcess();
		}
		futureTimerList.erase( iter );
	} else {
		timerList[timerSlot%slotCount].erase( iter );
	}
}

bool TimerSystem::executeTimer( TimeValue& remainingTime ) {
	getCurrentSystemTime( currentTime );
	sint32 targetSlot = getSlotNumber( currentTime );
	if ( currentSlot - targetSlot > 0 ) {
		ERROR(2)( Log::Error, "WARNING: clock has probably moved backwards by ", (currentSlot - targetSlot) * slotLength );
		currentSlot = targetSlot;
		currentFireList = timerList + currentSlot;
	} else if ( targetSlot - currentSlot > maxDeltaSlots ) {
		ERROR(2)( Log::Error, "WARNING: timer system overloaded, deviation is ", (targetSlot - currentSlot) * slotLength );
	}

	// fire all old timers, increase slot number until current slot is reached
	BaseTimer* nextAlarm = getNextAlarm();
	while ( targetSlot - currentSlot > 0 ) {
		while ( nextAlarm ) {
			LOG(5)( Log::Timer, "timer", nextAlarm, *nextAlarm, "fired late at time" , (DaytimeTimeValue&)currentTime );
			nextAlarm->internalFire();
			nextAlarm = getNextAlarm();
		}
		currentSlot += 1;
		currentFireList += 1;
		if ( currentFireList >= endOfList ) {
                 assert( currentSlot == slotCount && targetSlot >= slotCount );
			currentFireList = timerList;
			currentSlot = 0;
			epochBaseTime += totalPeriod;
                                        assert( epochBaseTime <= currentTime );
			targetSlot -= slotCount;
		}
		while( !futureTimerList.empty() ) {
			BaseTimer* t = futureTimerList.front();
			if ( (getSlotNumber( t->getAlarmTime() ) - currentSlot) < slotCount ) {
				futureTimerList.pop_front();
				t->reschedule();
				LOG(5)( Log::Timer, "timer", t, *t, "rescheduling into current epoch at time" , (DaytimeTimeValue&)currentTime );
			} else {
		break;
			}
		}
		nextAlarm = getNextAlarm();
	}

	// fuzzy timers: fire all timers of this slot!
	for (;;) {
		if ( nextAlarm ) {
#if !defined(FUZZY_TIMERS)
			remainingTime = nextAlarm->getAlarmTime() - currentTime;
			if ( remainingTime > timerResolution )
	break;
#endif
		}	else {
#if defined(FUZZY_TIMERS)
			// 'select' call will take minimum time of timerResolution anyway
			remainingTime = slotLength/2;
#else
			remainingTime = ( slotLength - currentTime % slotLength );
#endif
	break;
		}
		LOG(5)( Log::Timer, "timer", nextAlarm, *nextAlarm, "fired at time" , (DaytimeTimeValue&)currentTime );
		nextAlarm->internalFire();
		nextAlarm = getNextAlarm();
	}
	return true;
}

void TimerSystem::start() {
	TimeValue dummy;
	executeTimer( dummy );
}

TimeValue TimerSystem::checkDrift() {
	sint32 diff = getSlotNumber( getCurrentSystemTime() ) - currentSlot;
	return slotLength * ( diff > 0 ? diff -1 : diff );
}
