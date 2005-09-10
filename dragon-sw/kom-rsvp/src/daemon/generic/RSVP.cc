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
#include "RSVP.h"
#include "RSVP_API_Server.h"
#include "RSVP_ConfigFileReader.h"
#include "RSVP_Log.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_LogicalInterfaceSet.h"
#include "RSVP_List.h"
#include "RSVP_MPLS.h"
#include "RSVP_Message.h"
#include "RSVP_MessageProcessor.h"
#include "RSVP_NetworkService.h"
#include "RSVP_NetworkServiceDaemon.h"
#include "RSVP_PHopSB.h"
#include "RSVP_ProtocolObjects.h"
#include "RSVP_OIatPSB.h"
#include "RSVP_OutISB.h"
#include "RSVP_RSB.h"
#include "RSVP_RoutingService.h"
#include "RSVP_Session.h"
#include "RSVP_SignalHandling.h"
#include "force10_hack.h"

#if defined(WITH_API)
API_Server* RSVP::apiServer = NULL;
uint16 RSVP::apiPort = RSVP_Global::apiPort;
#endif

// global read-only settings
const FLOWSPEC_Object* RSVP::zeroFlowspec = NULL;

// define instances of state memory machines here
DEFINE_MEMORY_MACHINE( Session, sessionMemMachine );
DEFINE_MEMORY_MACHINE( PSB, psbMemMachine );
DEFINE_MEMORY_MACHINE( PHopSB, phopMemMachine );
DEFINE_MEMORY_MACHINE( OIatPSB, oiatpsbMemMachine );
DEFINE_MEMORY_MACHINE( RSB, rsbMemMachine );
DEFINE_MEMORY_MACHINE( OutISB, oisbMemMachine );
#if defined(REFRESH_REDUCTION)
DEFINE_MEMORY_MACHINE( Hop_RecvStorageIDListMemNode, idListMemMachine );
#endif

void RSVP::exitHandler( RSVP* This ) {
//	printSafe( "got exit signal..." );
	This->endFlag = true;
}

void RSVP::alarmHandler( RSVP* This ) {
}

RSVP::RSVP( const String& confFile, LogicalInterfaceList tmpLifList )
	: initOK(true), sessionHash(NULL), lifArray(NULL), hopListArray(NULL),
		interfaceCount(0), routing(NULL), endFlag(false),
		currentSessionCount(0), maxSessionCount(0), currentReservationCount(0) {

	// init global settings
	RSVP_Global::init();

#if defined(WITH_API)
	apiServer = NULL;
#endif

	if (RSVP_Global::currentTimerSystem || RSVP_Global::rsvp) {
		FATAL(1)( Log::Fatal, "ERROR: only one instance of RSVP allowed" );
		abortProcess();
	}

	// create routing object
	routing = new RoutingService();

	zeroFlowspec = new FLOWSPEC_Object;

	// set gobal RSVP context
	RSVP_Global::rsvp = this;

#if !defined(NS2)
	// query OS for existing network interfaces, result in tmpLifList
	NetworkServiceDaemon::buildInterfaceList( tmpLifList );
#endif

	// intialize routing, ask for additional interfaces (multicast vifs)
	routing->init( tmpLifList );

	mpls = new MPLS();

	// create main message processor
	RSVP_Global::messageProcessor = new MessageProcessor;

#if !defined(NS2)
	// read config file for additional information (interfaces, routing)
	ConfigFileReader* reader = NULL;
	if ( confFile != "" ) {
		reader = new ConfigFileReader( *this, tmpLifList );
		if ( !reader->parseConfigFile( confFile ) ) {
			delete reader;
			initOK = false;
	return;
		}
	}
#endif

//	RSVP_Global::reportSettings();

	sessionHash = new SessionHash(RSVP_Global::sessionHashCount);

#if defined(WITH_API)
	// create api server, create api interface with LIH = 0
	apiServer = new API_Server( apiPort );
	tmpLifList.push_front( const_cast<LogicalInterfaceAPI_Server*>(apiServer->getApiLif()) );
#else
	tmpLifList.push_front( new LogicalInterface( "dummy-api", 0, 0, 0 ) );
	tmpLifList.front()->disable();
#endif

	// dummy is needed to avoid 'Internal compiler error' with gcc 2.95.2
	uint32 dummy = tmpLifList.size();
	lifArray = new const LogicalInterface*[dummy];
	hopListArray = new HopList[dummy];

	// create timer system
	RSVP_Global::currentTimerSystem = new TimerSystem;

	LogicalInterfaceList::Iterator iter = tmpLifList.begin();
	for ( ;iter != tmpLifList.end(); ++iter ) {
		if ( !(*iter)->isDisabled() ) (*iter)->init( interfaceCount );
		lifArray[interfaceCount] = *iter;
		interfaceCount += 1;
	}
	iter = tmpLifList.begin();
	for ( ;iter != tmpLifList.end(); ++iter ) {
		if ( !(*iter)->isDisabled() ) {
			(*iter)->configureTC( NULL );
			LOG(2)( Log::Config, "interface:", **iter );
		}
	}

	// do additional routing initialization that need existance of interfaces
	routing->init2();

	mpls->init();
	// TODO: check for failure!

#if defined(RSVP_MEMORY_MACHINE)
	// only a limited number of messages exist in the system -> no pre-alloc
	// only a limited number of TSpec objects exist in the system -> no pre-alloc
	listMemMachine.addNodes( RSVP_Global::listAlloc );
	sessionMemMachine.addNodes( RSVP_Global::sbAlloc );
	psbMemMachine.addNodes( RSVP_Global::sbAlloc );
	phopMemMachine.addNodes( RSVP_Global::sbAlloc );
	oiatpsbMemMachine.addNodes( RSVP_Global::sbAlloc );
	rsbMemMachine.addNodes( RSVP_Global::sbAlloc );
	oisbMemMachine.addNodes( RSVP_Global::sbAlloc );
	obufMemMachine.addNodes( RSVP_Global::sbAlloc );
	Buffer::buf128memMachine.addNodes( RSVP_Global::sbAlloc );
	flowspecMemMachine.addNodes( RSVP_Global::sbAlloc * 4 );
	adspecMemMachine.addNodes( RSVP_Global::sbAlloc );
#endif /* RSVP_MEMORY_MACHINE */

	RSVP_Global::currentTimerSystem->start();

#if !defined(NS2)
	if (reader) {
		const SimpleList<ConfigFileReader::Hop>& hopList = reader->getHopList();
		SimpleList<ConfigFileReader::Hop>::ConstIterator hopIter = hopList.begin();
		for ( ; hopIter != hopList.end(); ++hopIter ) {
			const LogicalInterface* lif = findInterfaceByAddress( (*hopIter).iface );
			if ( !lif ) {
				ERROR(4)( Log::Error, "ignoring peer", (*hopIter).addr, "at unknown interface", (*hopIter).iface );
			} else {
				Hop* hop = findHop( *lif, (*hopIter).addr, true );
				if ( hop ) {
					hop->setStatic();
				}
			}
		}
	}
	delete reader;
#endif
}

RSVP::~RSVP() {
#if defined(WITH_API)
	if ( apiServer ) delete apiServer;
#endif
	if ( sessionHash ) {
		RSVP_Global::messageProcessor->prepareExit();
		uint32 x = 0;
		for ( ; x < RSVP_Global::sessionHashCount; ++x ) {
			SessionHash::HashBucket::ConstIterator iterSession = (*sessionHash)[x].begin();
			while ( iterSession != (*sessionHash)[x].end() ) {
				Session *s = *iterSession;
				++iterSession;
				s->deleteAll();
			}
		}
		delete sessionHash;
		LOG(1)( Log::Session, "all sessions removed" );
	}
	// Hop destructor needs access to MPLS object -> remove hops first
	for ( uint32 i = 0; i < interfaceCount; ++i ) {
		HopList::ConstIterator iter = hopListArray[i].begin();
		for ( ; iter != hopListArray[i].end(); ++iter ) {
			delete *iter;
		}
	}
	// MPLS destructor needs to access all interfaces -> delete MPLS object now
	if ( mpls ) delete mpls;

	if ( lifArray ) {
		for ( uint32 i = 0; i < interfaceCount; ++i ) {
			delete lifArray[i];
		}
		delete [] lifArray;
	}
	if ( hopListArray ) delete [] hopListArray;
	if ( RSVP_Global::messageProcessor ) delete RSVP_Global::messageProcessor;
	NetworkServiceDaemon::cleanup();
	if ( routing ) delete routing;
	if ( zeroFlowspec ) zeroFlowspec->destroy();
	if ( RSVP_Global::currentTimerSystem ) delete RSVP_Global::currentTimerSystem;

	force10_hack(NULL, NULL, "disengage");

	FATAL(1)( Log::Fatal, "RSVP: exiting gracefully" );
}	

#if defined(RSVP_STATS)
void RSVP::statsHandler( RSVP* This ) {
	This->logStats();
}
void RSVP::statsHandlerReset( RSVP* This ) {
	This->resetStats();
}
#endif

extern const char* VersionString();

int RSVP::main() {
#if !defined(NS2)
	SignalHandling::install( (SignalHandling::SigHandler)exitHandler, (SignalHandling::SigHandler)alarmHandler, (void*)this );
#if defined(RSVP_STATS)
	SignalHandling::installUserSignal( (SignalHandling::SigHandler)statsHandler, (SignalHandling::SigHandler)statsHandlerReset );
#endif

	FATAL(2)( Log::Fatal, "RSVPD running -", VersionString() );

	while (!endFlag) {
		const LogicalInterface* currentLif = NetworkServiceDaemon::queryInterfaces();
		if ( currentLif ) {
                                           assert( !currentLif->isDisabled() );
			RSVP_Global::messageProcessor->readCurrentMessage( *currentLif );
		} else if ( NetworkServiceDaemon::queryAndClearAsyncUnicastRouting() ) {
			NetAddress dest, gateway;
			const LogicalInterface* outLif = routing->getAsyncUnicastRoutingEvent( dest, gateway );
			if ( outLif ) {
				SESSION_Object session( dest, 0, 0);
				SessionHash::HashBucket::Iterator sessionIter = sessionHash->lower_bound( &session );
				for ( ; sessionIter != sessionHash->getHashBucket(&session).end() && (*sessionIter)->getDestAddress() == dest; ++sessionIter ) {
					RSVP_Global::messageProcessor->processAsyncRoutingEvent( *sessionIter, *outLif, gateway );
				}
			}
		} else if ( NetworkServiceDaemon::queryAndClearAsyncMulticastRouting() ) {
			NetAddress src, dest;
			const LogicalInterface* inLif = NULL;
			LogicalInterfaceSet lifList;
			if ( routing->getAsyncMulticastRoutingEvent( src, dest, inLif, lifList ) ) {
				SESSION_Object session( dest, 0, 0);
				SessionHash::HashBucket::Iterator sessionIter = sessionHash->lower_bound( &session );
				for ( ; sessionIter != sessionHash->getHashBucket(&session).end() && (*sessionIter)->getDestAddress() == dest; ++sessionIter ) {
					RSVP_Global::messageProcessor->processAsyncRoutingEvent( *sessionIter, src, *inLif, lifList );
				}
			}
		} else if ( !endFlag ) {
			FATAL(1)( Log::Fatal, "returned from queryInterfaces but without result" );
			abortProcess();
		}
	}
#endif /* NS2 */
	return 0;
}

const LogicalInterface* RSVP::findInterfaceByName( const String& name ) const {
	uint32 i;
	for ( i = 0; i < interfaceCount; ++i ) {
		if ( lifArray[i]->getName().leftequal( name ) ) {
			return lifArray[i];
		}
	}
	return NULL;
}

const LogicalInterface* RSVP::findInterfaceByAddress( const NetAddress& addr ) const {
	uint32 i;
	for ( i = 0; i < interfaceCount; ++i ) {
		if ( lifArray[i]->getLocalAddress() == addr ) {
			return lifArray[i];
		}
	}
	return NULL;
}

#if defined(NS2)
const LogicalInterface* RSVP::findInterfaceByOif( const String& oif ) const {
	uint32 i;
	for ( i = 0; i < interfaceCount; ++i ) {
		if ( lifArray[i]->getOif() == oif ) {
			return lifArray[i];
		}
	}
	return NULL;
}
#endif

Session* RSVP::findSession( const SESSION_Object& session, bool createIfNotFound ) {
	SessionHash::HashBucket::Iterator iter = sessionHash->lower_bound( const_cast<SESSION_Object*>(&session) );
	if ( iter != sessionHash->getHashBucket(const_cast<SESSION_Object*>(&session)).end() && **iter == session ) {
		LOG(2)( Log::Session, "found Session:", **iter );
		return *iter;
	}
	if ( createIfNotFound ) {
		Session* conflictCandidate = NULL;
		if ( session.getTunnelId() == 0 ) {
			if ( iter != sessionHash->getHashBucket(const_cast<SESSION_Object*>(&session)).end() ) {
				conflictCandidate = *iter;
			}
		} else {
			if ( iter != sessionHash->getHashBucket(const_cast<SESSION_Object*>(&session)).begin() ) {
				conflictCandidate = *(iter.prev());
			}
		}

		if ( conflictCandidate
			&& conflictCandidate->getDestAddress() == session.getDestAddress()
			&& conflictCandidate->getTunnelId() == session.getTunnelId()
			&& conflictCandidate->getExtendedTunnelId() == session.getExtendedTunnelId() ) {
	return NULL;
		}
		else {
			Session* newSession = new Session( session );
			increaseSessionCount();
			newSession->setIterFromRSVP( sessionHash->insert( iter, newSession ) );
	return newSession;
		}
	}
	return NULL;
}

void RSVP::removeSession( SessionHash::Iterator iter ) {
	sessionHash->erase( iter );
	decreaseSessionCount();
}

Hop* RSVP::findHop( const LogicalInterface& lif, const NetAddress& addr, bool create ) {
	HopKey key( lif, addr );
	HopList::ConstIterator iter = hopListArray[lif.getLIH()].lower_bound( &key );
	if ( iter != hopListArray[lif.getLIH()].end() && **iter == key ) {
		return *iter;
	} else if ( create ) {
		Hop* hop = new Hop(lif,addr);
		hopListArray[lif.getLIH()].insert( iter, hop );
		return hop;
	} else {
		return NULL;
	}
}

void RSVP::removeHop( HopKey& key ) {
	HopList::ConstIterator iter = hopListArray[key.getLIH()].find( &key );
	if ( iter != hopListArray[key.getLIH()].end() ) {
		Hop* hop = *iter;
		hopListArray[key.getLIH()].erase( iter );
		delete hop;
	}
}

inline void RSVP::increaseSessionCount() {
#if defined(RSVP_STATS) 
	currentSessionCount += 1;
	if ( currentSessionCount > maxSessionCount ) {
		maxSessionCount = currentSessionCount;
	}
#ifdef SunOS
// only for testing!!
//	if ( currentSessionCount > currentReservationCount + 5 * interfaceCount ) {
//		logStats();
//		exitHandler( this );
//	}
#endif
#endif
}

inline void RSVP::decreaseSessionCount() {
#if defined(RSVP_STATS)
	currentSessionCount -= 1;
#endif
}

inline void RSVP::logStats() {
#if defined(RSVP_STATS)
	printSafe( "current session count is %d\n", currentSessionCount );
	printSafe( "max session count is %d\n", maxSessionCount );
	printSafe( "current reservation count is %d\n", currentReservationCount );
#if defined(WITH_API)
	printSafe( "number of API clients is %d\n", getApiServer().getNumberOfClients() );
#endif
#endif
}

inline void RSVP::resetStats() {
#if defined(RSVP_STATS)
	currentSessionCount = 0;
	maxSessionCount = 0;
	currentReservationCount = 0;
#endif
}
