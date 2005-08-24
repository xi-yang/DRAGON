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
#include "RSVP_LogicalInterface.h"
#include "RSVP_Hop.h"
#include "RSVP_Message.h"
#include "RSVP_NetworkServiceDaemon.h"
#include "RSVP_PacketHeader.h"
#include "RSVP_Scheduler.h"
#include "RSVP_TrafficControl.h"
#if defined(NS2)
#include "RSVP_Global.h"
#include "RSVP_Daemon_Wrapper.h"
#endif

void LogicalInterface::init( uint32 LIH ) {
	if ( disabled ) return;
#if !defined(NS2)
	fd = NetworkServiceDaemon::initRawInterfaceIP4( getAddress() );
	NetworkServiceDaemon::initRawUnicastInterfaceIP4( fd, getAddress() );
	if ( vif != -1 ) {
		NetworkServiceDaemon::initRawMulticastInterfaceIP4( fd, getAddress(), vif );
		name += String("(vif ") + convertIntToString( vif ) + ")";
	}
#endif
	RSVP_HOP_Object::setLIH( LIH );
}

void LogicalInterface::configureTC( TrafficControl* tc ) {
	if ( !trafficControl ) {
		if ( !tc ) {
			trafficControl = new TrafficControl( new SchedulerDummy( 0 ,0 ) );
		} else {
			trafficControl = tc;
		}
	} else if ( !tc ) {
		clonedTC = trafficControl->configure( *this );
	} else {
		delete tc;
	}
}

const LogicalInterface* LogicalInterface::receiveBuffer( INetworkBuffer& buf, PacketHeader& header ) const {
	if ( disabled ) return NULL;
#if defined(NS2)
	const LogicalInterface* realLif = NULL;
                                                  assert(RSVP_Global::wrapper);
	RSVP_Global::wrapper->receivePacket( buf );
#else
	const LogicalInterface* realLif = NetworkServiceDaemon::receiveRawPacketIP4( fd, buf );
	if ( buf.getRemainingSize() == 0 ) {
		return NULL;
	}
#endif
	buf >> header;
	return realLif ? realLif : this;
}

void LogicalInterface::sendBuffer( const ONetworkBuffer& obuf, const NetAddress& dest, const NetAddress& gw ) const {
#if defined(NS2)
                                                  assert(RSVP_Global::wrapper);
	nsaddr_t destNode = reinterpret_cast<RSVP_Daemon_Wrapper*>(RSVP_Global::wrapper)->mapInterfaceToNode( dest );
	RSVP_Global::wrapper->sendPacket( obuf, destNode, 0 );
#else
	if ( obuf.getUsedSize() > MTU ) {
		// this can affect probably only FF-style RESV messages
		// check with RFC 2205 whether fragementation is allowed at all
		// otherwise decompose in 'MessageProcessor::refreshReservations'
		// and store appropriately in PHopSB
		ERROR(5)( Log::Error, "ERROR: message buffer is", obuf.getUsedSize(), ", longer than MTU", MTU, ", but fragmentation is not implemented, not sending message");
	} else {
		NetworkServiceDaemon::sendRawPacketIP4( fd, obuf, dest, gw );
	}
#endif
}
