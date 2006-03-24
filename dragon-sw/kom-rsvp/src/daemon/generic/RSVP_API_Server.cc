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
#if defined(WITH_API)

#include "RSVP_API_Server.h"

#include "RSVP.h"
#include "RSVP_Global.h"
#include "RSVP_Hop.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_Message.h"
#include "RSVP_MessageProcessor.h"
#include "RSVP_ProtocolObjects.h"
#include "RSVP_TrafficControl.h"
#include "RSVP_RoutingService.h"

//#if defined(NS2)
//#include "RSVP_Wrapper.h"
//#endif

inline void API_Entry::timeout() {
	RSVP::getApiServer().timeoutAPI( session, address, port );
}

inline void API_Entry::refresh() {
	RSVP::getApiServer().requestRefreshAPI( session, address, port );
}

inline ApiEntryList& API_Server::getApiList( const SESSION_Object& s ) {
	return apiEntryList[s.getHashValue(RSVP_Global::apiHashCount)];
}

inline const ApiEntryList& API_Server::getApiList( const SESSION_Object& s ) const {
	return apiEntryList[s.getHashValue(RSVP_Global::apiHashCount)];
}

API_Server::API_Server( uint16 port ) : currentProcessor(NULL), currentPort(0),
		currentAddress(NULL), clientCount(0) {
	apiEntryList = new ApiEntryList[RSVP_Global::apiHashCount];
	apiLif = new LogicalInterfaceAPI_Server( RSVP_Global::apiName, 0, RSVP_Global::apiMTU );
	PortList destPortList;
	apiLif->configureUDP( port, NetAddress(0), destPortList );
	apiLif->configureRefresh( 0 );
	apiLif->configureTC( new TrafficControl(NULL) );
}

API_Server::~API_Server() {
	uint32 x = 0;
	currentProcessor = RSVP_Global::messageProcessor;
	for ( ; x < RSVP_Global::apiHashCount; ++x ) {
		ApiEntryList::ConstIterator iter = apiEntryList[x].begin();
		while ( iter != apiEntryList[x].end() ) {
			ApiEntryList::ConstIterator nextIter = iter.next();
			deregisterAPI( iter );
			iter = nextIter;
		}
	}
	delete [] apiEntryList;
}

void API_Server::deregisterAPI( ApiEntryList::ConstIterator iter ) {
	API_Entry *entry = *iter;
	LOG(6)( Log::API, "removing API:", entry->session, "for API at", entry->address, "/", entry->port );
	getApiList( entry->session ).erase( iter );
	currentProcessor->deregisterAPI( entry->session, entry->address, entry->port );
	delete entry;
#if defined(RSVP_STATS)
	clientCount -= 1;
#endif
}

inline void API_Server::timeoutAPI( const SESSION_Object& session, const NetAddress& address, uint16 port ) {
	API_EntryKey key(session,address,port);
	ApiEntryList::ConstIterator iter = getApiList( session ).find( &key );
	if ( iter != getApiList( session ).end() ) {
		currentProcessor = RSVP_Global::messageProcessor;
		currentAddress = &address;
		currentPort = port;
		ERROR(6)( Log::Error, "API timeout:", session, "for API at", address, "/", port );
		deregisterAPI( iter );
		currentPort = 0;
		currentProcessor = NULL;
	}
}

inline void API_Server::requestRefreshAPI( const SESSION_Object& session, const NetAddress& address, uint16 port ) {
	Message msg( Message::InitAPI, 127, session );
	msg.setRSVP_HOP_Object( *apiLif );
	currentAddress = &address;
	currentPort = port;
	sendMessage( msg );
	currentPort = 0;
}

void API_Server::processMessage( const Message& msg, MessageProcessor& mp) {
       assert( msg.getMsgType() == Message::InitAPI  || msg.getMsgType() == Message::RemoveAPI);
	currentProcessor = &mp;
	SESSION_Object session = msg.getSESSION_Object();
	uint16 port = msg.getRSVP_HOP_Object().getLIH();
	const NetAddress& address = msg.getRSVP_HOP_Object().getAddress();

	//Initialize OSPF socket 
	if (!RSVP_Global::rsvp->getRoutingService().getOspfSocket()){
		if (!RSVP_Global::rsvp->getRoutingService().ospf_socket_init()){
			ERROR(1)( Log::Error, "Failed to init rsvp-ospf communication!" );
			RSVP_Global::rsvp->getRoutingService().disableOspfSocket();
		}
	}

	if (msg.getMsgType() == Message::InitAPI && (session.getDestAddress() == LogicalInterface::loopbackAddress 
		|| session.getDestAddress() == RSVP_Global::rsvp->getRoutingService().getLoopbackAddress()))
	{
		//register default client API
		ApiEntryList::ConstIterator iter1 = getApiList( session ).begin();
		while ( iter1 != getApiList( session ).end() ) {
			ApiEntryList::ConstIterator nextIter = iter1.next();
			if ((*iter1)->session == session && (*iter1)->address == address && (*iter1)->port!=port)
				deregisterAPI( iter1 );
			iter1 = nextIter;
		}
	}
	API_EntryKey key(session,address,port);
  	ApiEntryList::ConstIterator iter = getApiList( session ).find( &key );
	if ( msg.getMsgType() == Message::InitAPI ) {
	  if ( iter == getApiList( session ).end() ) {
			TimeValue apiRefresh = RSVP_Global::defaultApiRefresh;
			if ( msg.hasTIME_VALUES_Object() ) {
				apiRefresh = msg.getTIME_VALUES_Object().getRefreshTime();
			}
			iter = getApiList( session ).insert_sorted( new API_Entry(session,address,port,apiRefresh) );
			LOG(6)( Log::API, "registered", session, "for API at", address, "/", port );
			currentAddress = &address;
			currentPort = port;
			currentProcessor->registerAPI( session );
			currentPort = 0;
#if defined(RSVP_STATS)
			clientCount += 1;
#endif
		} else {
			if ( msg.hasTIME_VALUES_Object() ) {
				(*iter)->restartTimeout( msg.getTIME_VALUES_Object().getRefreshTime() );
			} else {
				(*iter)->restartTimeout();
			}
			LOG(6)( Log::API, "refreshed", session, "for API at", address, "/", port );
		}
	} else {
		if ( iter == getApiList( session ).end() ) {
			LOG(6)( Log::API, "UNKNOWN", session, "for API at", address, "/", port );
		} else {
			deregisterAPI( iter );
		}
	}
	currentProcessor = NULL;
}

void API_Server::sendMessage( const Message& msg ) {
	if ( currentPort ) {
		apiLif->setDestPort( currentPort );
		apiLif->sendMessageInternal( msg, *currentAddress, apiLif->getAddress() );
	} else {
		bool apiFound = false;
		API_EntryKey key( msg.getSESSION_Object(), NetAddress(0), 0 );
		ApiEntryList::ConstIterator iter = getApiList( key.getSession() ).lower_bound( &key );
		for ( ; iter != getApiList( key.getSession() ).end() && (*iter)->session == key.getSession(); ++iter ) {
			apiFound = true;
			apiLif->setDestPort( (*iter)->port );
			apiLif->sendMessageInternal( msg, (*iter)->address, apiLif->getAddress() );
		}
		if ( !apiFound ) {
			LOG(2)( Log::API, "WARNING: no API client found for, sending to default API client", msg.getSESSION_Object() );
			apiFound = false;
			SESSION_Object defaultApiSession(LogicalInterface::loopbackAddress, 0, 0);
			API_EntryKey key1( defaultApiSession, NetAddress(0), 0 );
			ApiEntryList::ConstIterator iter = getApiList( key1.getSession() ).lower_bound( &key1 );
			for ( ; iter != getApiList( key1.getSession() ).end() && (*iter)->session == key1.getSession(); ++iter ) {
				apiFound = true;
				apiLif->setDestPort( (*iter)->port );
				apiLif->sendMessageInternal( msg, (*iter)->address, apiLif->getAddress() );
			}
			if ( !apiFound ) {
				LOG(2)( Log::API, "WARNING: no default API client found, unable to send message", defaultApiSession);
			}
		}
	}
}

bool API_Server::findApiSession( const SESSION_Object& session ) const {
	API_EntryKey key( session, NetAddress(0), 0 );
	ApiEntryList::ConstIterator iter = getApiList( session ).lower_bound( &key );
	return iter != getApiList( session ).end() && (*iter)->session == session;
}

void LogicalInterfaceAPI_Server::sendMessage( const Message& msg, const NetAddress&, const NetAddress&, const NetAddress& ) const {
	RSVP::getApiServer().sendMessage( msg );
}

#endif /* WITH_API */
