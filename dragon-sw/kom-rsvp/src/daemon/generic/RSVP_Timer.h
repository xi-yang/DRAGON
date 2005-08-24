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
#ifndef _RSVP_Timer_h_
#define _RSVP_Timer_h_ 1

#include "RSVP_BaseTimer.h"

template <class T>
class RefreshTimer : public BaseTimer	{
	TimeValue period;
	T& stateBlock;
protected:
	virtual void internalFire() {
		cancel();
		alarmTime += period;
		start();
		stateBlock.refresh();
	}
public:
	RefreshTimer( T& stateBlock, const TimeValue& refreshTime = TimeValue(0,0) )
		: BaseTimer(refreshTime), period(refreshTime), stateBlock(stateBlock) {}
	void restart() {
		BaseTimer::restart(period);
	}
	void restart( const TimeValue& refreshTime ) {
		period = refreshTime;
		BaseTimer::restart(refreshTime);
	}
	const TimeValue& getPeriod() const {
		return period;
	}
};

template <class T>
class RandomRefreshTimer : public BaseTimer	{
	TimeValue period;
	T& stateBlock;
protected:
	virtual void internalFire() {
		cancel();
		alarmTime += randomizeRefreshTime( period );
		start();
		stateBlock.refresh();
	}
public:
	RandomRefreshTimer( T& stateBlock, const TimeValue& refreshTime = TimeValue(0,0) ) 
	: BaseTimer(randomizeRefreshTime(refreshTime)), period(refreshTime),
		stateBlock(stateBlock) {}
	void restart( const TimeValue& refreshTime ) {
		period = refreshTime;
		BaseTimer::restart( randomizeRefreshTime( refreshTime ) );
	}
	const TimeValue& getPeriod() const {
		return period;
	}
	void restartOnce( const TimeValue& refreshTime ) {
		BaseTimer::restart(refreshTime);
	}
};

template <class T>
class TimeoutTimer : public BaseTimer	{
	TimeValue timeout;
	T& stateBlock;
protected:
	virtual void internalFire() {
		stateBlock.timeout();
	}
public:
	TimeoutTimer( T& stateBlock, const TimeValue& timeoutTime = TimeValue(0,0) )
		: BaseTimer(timeoutTime), timeout(timeoutTime), stateBlock(stateBlock) {}
	void restart() {
		BaseTimer::restart( timeout );
	}
	void restart( const TimeValue& t ) {
		timeout = t;
		BaseTimer::restart( timeout );
	}
	const TimeValue& getTimeout() const {
		return timeout;
	}
};

#endif /* _RSVP_Timer_h_ */
