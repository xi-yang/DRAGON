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
#include "RSVP_Global.h"
#include "RSVP_BaseTimer.h"
#include "RSVP_LogicalInterface.h"
#if defined(RSVP_MEMORY_MACHINE)
#include "RSVP_FilterSpecList.h"
#include "RSVP_IntServObjects.h"
#include "RSVP_Message.h"
#endif

const TimeValue TimerSystem::W(1,0);
const TimeValue LogicalInterface::defaultRefresh(30,0);
#if defined(REFRESH_REDUCTION)
const TimeValue LogicalInterface::defaultRapidRefresh(0,500000);
#endif
const NetAddress LogicalInterface::loopbackAddress("127.0.0.1");
const NetAddress LogicalInterface::noGatewayAddress("0.0.0.0");
const TimeValue RSVP_Global::defaultApiRefresh(120,0);
const char* const RSVP_Global::apiName = "rsvp-api";

// configuration of hashed fuzzy timer system
sint32 TimerSystem::slotCount = 0;
TimeValue TimerSystem::totalPeriod = TimeValue( TIMER_SLOT_TOTAL_PERIOD, 0 );
TimeValue TimerSystem::slotLength = TimeValue( 0, 0 );
TimeValue TimerSystem::timerResolution = TimeValue(0,0);
sint32 TimerSystem::maxDeltaSlots = 0;

// configuration of hashed session container
uint32 RSVP_Global::sessionHashCount = SESSION_HASH_COUNT;

// configuration of hashed api container -> only needed for tests
uint32 RSVP_Global::apiHashCount = API_HASH_COUNT;

// configuration of hashed message id containers -> for refresh reduction
uint32 RSVP_Global::idHashCountSend = MESSAGE_ID_HASH_COUNT_SEND;

// configuration of hashed message id containers -> for refresh reduction
uint32 RSVP_Global::idHashCountRecv = MESSAGE_ID_HASH_COUNT_RECV;

// pre-allocation of list nodes for list memory machine
uint32 RSVP_Global::listAlloc = 0;

// pre-allocation of state blocks for state blocks memory machine
uint32 RSVP_Global::sbAlloc = 0;

RSVP*             RSVP_Global::rsvp = NULL;
SNMP_Global*  RSVP_Global::snmp = NULL;
MessageProcessor* RSVP_Global::messageProcessor = NULL;
TimerSystem*      RSVP_Global::currentTimerSystem = NULL;
#if defined(NS2)
RSVP_Wrapper*			RSVP_Global::wrapper = NULL;
#endif

// default setting whether to enable MPLS per interface (if possible)
#if MPLS_REAL
bool							RSVP_Global::mplsDefault = true;
#else
bool							RSVP_Global::mplsDefault = false;
#endif

// default size of label hash container for MPLS */
uint32						RSVP_Global::labelHashCount = LABEL_HASH_COUNT;

// define instances of memory machines
DEFINE_MEMORY_MACHINE( ListMemNode, listMemMachine )
DEFINE_MEMORY_MACHINE( FILTER_SPEC_ObjectListMemNode, filterSpecListMemMachine )
DEFINE_MEMORY_MACHINE( FlowDescriptorListMemNode, flowDescListMemMachine )
DEFINE_MEMORY_MACHINE( Message, msgMemMachine )
DEFINE_MEMORY_MACHINE( FLOWSPEC_Object, flowspecMemMachine )
DEFINE_MEMORY_MACHINE( ADSPEC_Object, adspecMemMachine )
DEFINE_MEMORY_MACHINE( ONetworkBuffer, obufMemMachine )
DEFINE_MEMORY_MACHINE( Buffer::BufferNode128, Buffer::buf128memMachine )
#if defined(REFRESH_REDUCTION)
DEFINE_MEMORY_MACHINE( MESSAGE_ID_ACK_ObjectListMemNode, msgIdAckListMemMachine )
DEFINE_MEMORY_MACHINE( MESSAGE_ID_NACK_ObjectListMemNode, msgIdNackListMemMachine )
#endif

// ensure that above constructors are called and do some consistency checks
void RSVP_Global::init() {
	initSystem();
}

void RSVP_Global::reportSettings() {
	cerr << "sessionHashCount:" << sessionHashCount << endl;
	cerr << "apiHashCount:" << apiHashCount << endl;
	cerr << "idHashCountSend:" << idHashCountSend << endl;
	cerr << "idHashCountRecv:" << idHashCountRecv << endl;
}

const TimeValue& RSVP_Global::getCurrentTime() {
	return currentTimerSystem->getCurrentTime();
}
