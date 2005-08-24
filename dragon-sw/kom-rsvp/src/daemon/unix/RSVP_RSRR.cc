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
#if defined(REAL_NETWORK)

#include "RSVP_RSRR.h"
#include "RSVP_BasicTypes.h"
#include "RSVP_Log.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_NetworkService.h"
#include "RSVP_NetworkServiceDaemon.h"
#include "SystemCallCheck.h"
#include "rsrr.h"

#include <unistd.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>

static char rsrr_recv_buf[RSRR_MAX_LEN]; /* RSRR receive buffer */
static char rsrr_send_buf[RSRR_MAX_LEN]; /* RSRR send buffer */
static struct sockaddr_un serv_addr;     /* Server address */
static struct sockaddr_un cli_addr;      /* Client address */
static int clilen, servlen;              /* Lengths */
static int rsrr_socket = -1;
static int expected_type = RSRR_ALL_TYPES;
static struct rsrr_header* rsrr = NULL;
static unsigned long queryCounter = 0;

class LogicalInterface;
static const LogicalInterface* ifaceArray[RSRR_MAX_VIFS];

static const LogicalInterface** replyInLif;
static LogicalInterfaceSet* replyOutLifs;
static NetAddress replySourceAddress;
static NetAddress replyDestAddress;

LogicalInterfaceList* initLifList = NULL;

RSRR::RSRR() {
                                                  assert( rsrr_socket == -1 );
	rsrr_socket = CHECK( socket(AF_UNIX, SOCK_DGRAM, 0) );

	initMemoryWithZero( &serv_addr, sizeof(serv_addr) );
	serv_addr.sun_family = AF_UNIX;
	strcpy( serv_addr.sun_path, RSRR_SERV_PATH );
	servlen = ( offsetof(struct sockaddr_un, sun_path) + strlen(serv_addr.sun_path) );

	initMemoryWithZero( &cli_addr, sizeof(cli_addr) );
	cli_addr.sun_family = AF_UNIX;
	strcpy(cli_addr.sun_path, RSRR_CLI_PATH);
	clilen = ( offsetof(struct sockaddr_un, sun_path) + strlen(cli_addr.sun_path) );

	unlink(cli_addr.sun_path);
	CHECK( bind( rsrr_socket, (struct sockaddr *) &cli_addr, clilen) );
}

RSRR::~RSRR() {
	if ( rsrr_socket != -1 ) {
		NetworkServiceDaemon::deregisterRSRR_Handle( rsrr_socket );
		close( rsrr_socket );
		rsrr_socket = -1;
	}
	unlink(cli_addr.sun_path);
}

bool RSRR::init( LogicalInterfaceList& tmpLifList ) {
	initLifList = &tmpLifList;
	rsrr = (struct rsrr_header*)rsrr_send_buf;
	rsrr->version = 1;
	rsrr->type = RSRR_INITIAL_QUERY;
	rsrr->flags = 0;
	rsrr->num = 0;  
	int sendlen = RSRR_HEADER_LEN;
	LOG(1)( Log::RSRR, "sending RSRR query" );
	if ( sendto(rsrr_socket, (SENDTO_BUF_T)rsrr_send_buf, sendlen, 0, (struct sockaddr *)&serv_addr, servlen) < 0 ) {
		return false;
	}
	expected_type = RSRR_INITIAL_REPLY;
	NetworkServiceDaemon::registerRSRR_Handle( rsrr_socket );
	return processMessage();
}

bool RSRR::processMessage() {
	RECVFROM_SIZE_T dummy;
	unsigned int recvlen = CHECK( recvfrom(rsrr_socket, (RECVFROM_BUF_T)rsrr_recv_buf, sizeof(rsrr_recv_buf),0 ,(struct sockaddr *) 0, &dummy) );
	if ( recvlen < RSRR_HEADER_LEN) {
		ERROR(4)( Log::Error, "ERROR: received RSRR packet of length", recvlen, "expected", RSRR_HEADER_LEN );
		return false;
	}
	rsrr = (struct rsrr_header*)rsrr_recv_buf;
	if (rsrr->version > RSRR_MAX_VERSION) {
		ERROR(4)( Log::Error, "ERROR: received RSRR packet of version", (u_short)rsrr->version, "can handle only version", RSRR_MAX_VERSION );
		return false;
	}
	if ( expected_type != RSRR_ALL_TYPES && rsrr->type != (u_char)expected_type) {
		ERROR(4)( Log::Error, "ERROR: received RSRR packet of type", (u_short)rsrr->type, "expected", (u_short)expected_type );
		return false;
	}
	switch (rsrr->type) {
	case RSRR_INITIAL_REPLY:
		if ( recvlen < (RSRR_HEADER_LEN + rsrr->num*RSRR_VIF_LEN) ) {
			ERROR(4)( Log::Error, "ERROR: received RSRR_INITIAL_REPLY of length", recvlen, "expected", RSRR_HEADER_LEN + rsrr->num*RSRR_VIF_LEN );
			return false;
		}
		acceptInitialReply();
		break;
	case RSRR_ROUTE_REPLY:
		if ( recvlen < RSRR_RR_LEN) {
			ERROR(4)( Log::Error, "ERROR: received RSRR_ROUTE_REPLY of length", recvlen, "expected", RSRR_RR_LEN );
			return false;
		}
		acceptRouteReply();
		break;
	default:
		ERROR(2)( Log::Error, "ERROR: received RSRR packet of unknown type", rsrr->type );
		return false;
	}
	expected_type = RSRR_ALL_TYPES;
	return true;
}

void RSRR::acceptInitialReply() {
	int vif_num = rsrr->num;
	LOG(3)( Log::RSRR, "RSRR: got", vif_num, "vifs" );
	struct rsrr_vif *vifs = (struct rsrr_vif *) (rsrr_recv_buf + RSRR_HEADER_LEN);
	int i = 0;
	for (; i < vif_num; i++) {
		if ( !(vifs[i].status & (1 << RSRR_DISABLED_BIT)) ) {
			NetAddress addr = NetAddress( vifs[i].local_addr.s_addr );
			LOG(4)( Log::RSRR, "got vif with local addr:", addr , "id:", (uint16)vifs[i].id );
			const LogicalInterface* lif = NULL;
			LogicalInterfaceList::Iterator iter = initLifList->begin();
			for ( ; iter != initLifList->end(); ++iter ) {
				if ( (*iter)->getLocalAddress() == addr ) {
					lif = *iter;
			break;
				}
			}
			if ( !lif ) {
	continue;
			}
#if defined(FreeBSD)
			if ( lif->hasVif() ) {
				// already a vif with this local address -> new one must be a tunnel
				LogicalInterface* newLif = new LogicalInterface( lif->getName(), addr, NetAddress("240.0.0.0"), lif->getMTU() );
				initLifList->push_back( newLif );
				lif = newLif;
			}
#endif
			ifaceArray[vifs[i].id] = lif;
			const_cast<LogicalInterface*>(lif)->setVif( (VifHandle)vifs[i].id );
		} else {
			ifaceArray[vifs[i].id] = NULL;
		}
	}
}

void RSRR::acceptRouteReply() {
	struct rsrr_rr* route_reply = (struct rsrr_rr *) (rsrr_recv_buf + RSRR_HEADER_LEN);
	if ( route_reply->query_id != queryCounter ) {
		// this is an async route notification -> should be buffered and delivered later
	}
	*replyInLif = ifaceArray[route_reply->in_vif];
	replySourceAddress = route_reply->source_addr.s_addr;
	replyDestAddress = route_reply->dest_addr.s_addr;
#if defined(LOG_ON)
	static String name = "";
	if (*replyInLif) name = (*replyInLif)->getName();
	LOG(7)( Log::RSRR, "RSRR reply for:", replySourceAddress, "->", replyDestAddress, "in:", name, "out:" );
#endif
	int i = 0;
	for (; i < RSRR_MAX_VIFS; i++) {
		if ( route_reply->out_vif_bm & (1 << i) ) {
			LOG(C)( Log::RSRR, ifaceArray[i]->getName() );
			replyOutLifs->insert_unique( ifaceArray[i] );
		}
	}
	LOG(C)( Log::RSRR, endl );
}

void RSRR::getRoute( const NetAddress& src, const NetAddress& dest, const LogicalInterface*& inLif, LogicalInterfaceSet& lifList ) {
	rsrr = (struct rsrr_header*)rsrr_send_buf;
	rsrr->version = 1;
	rsrr->type = RSRR_ROUTE_QUERY;
	rsrr->flags |= (1 << RSRR_NOTIFICATION_BIT);
	rsrr->num = 0;
	struct rsrr_rq *route_query = (struct rsrr_rq *) (rsrr_send_buf + RSRR_HEADER_LEN);
	route_query->dest_addr.s_addr = dest.rawAddress();
	route_query->source_addr.s_addr = src.rawAddress();
	route_query->query_id = ++queryCounter;
	int sendlen = RSRR_RQ_LEN;
	LOG(3)( Log::RSRR, "sending RSRR request:", src, dest );
	CHECK( sendto(rsrr_socket, (SENDTO_BUF_T)rsrr_send_buf, sendlen, 0, (struct sockaddr *)&serv_addr, servlen));
	// remove this loop when async route replies can be buffered
	LogicalInterfaceSet storedLifList = lifList;
	do {
		inLif = NULL;
		replyInLif = &inLif;
		lifList = storedLifList;
		replyOutLifs = &lifList;
		expected_type = RSRR_ROUTE_REPLY;
		processMessage();
	} while ( dest != replyDestAddress || src != replySourceAddress );
}

bool RSRR::getAsyncRoutingEvent( NetAddress& src, NetAddress& dest, const LogicalInterface*& inLif, LogicalInterfaceSet& lifList ) {
	LOG(1)( Log::Routing, "checking for asynchronous multicast routing info" );
	inLif = NULL;
	replyInLif = &inLif;
	replyOutLifs = &lifList;
	expected_type = RSRR_ROUTE_REPLY;
	processMessage();
	src = replySourceAddress;
	dest = replyDestAddress;
	return true;
}

#endif
