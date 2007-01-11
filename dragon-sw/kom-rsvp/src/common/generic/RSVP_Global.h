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
#ifndef _RSVP_Global_h_
#define _RSVP_Global_h_ 1

#include "RSVP_BasicTypes.h"
#include "RSVP_TimeValue.h"

class RSVP;
class SwitchCtrl_Global;
class LogicalInterface;
class TimerSystem;
class MessageProcessor;
#if defined(NS2)   
class RSVP_Wrapper;
#endif

// please look into RSVP_Global.cc for comments

struct RSVP_Global {
	static void init();
	static void reportSettings();
	static RSVP* rsvp;
	static SwitchCtrl_Global* switchController;
	static MessageProcessor* messageProcessor;
	static TimerSystem* currentTimerSystem;
#if defined(NS2)   
	static RSVP_Wrapper* wrapper;
#endif
	static uint32 sessionHashCount;
	static uint32 apiHashCount;
	static uint32 idHashCountSend;
	static uint32 idHashCountRecv;
	static uint32 listAlloc;
	static uint32 sbAlloc;
	static uint32 labelHashCount;
	static bool mplsDefault;
	// API related global constants
	static const uint16 apiPort = 4000;
	static const char* const apiName;
	static const char* const apiUniClientName;
	static const uint16 apiMTU = 8191;
	static const TimeValue defaultApiRefresh;
	// needed for class LogicalInterfaceSet
	static const uint32 maxNumberOfInterfaces = 32;
	static const TimeValue& getCurrentTime();
};

#endif /* _RSVP_Global_h_ */
