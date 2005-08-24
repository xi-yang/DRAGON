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
#include "rsvp-agent.h"

#include "RSVP_Message.h"
#include "RSVP_PacketHeader.h"

#include "packet.h"
#include "ip.h"
#include "scheduler.h"

int RSVP_Agent::command(int argc, const char*const* argv) {
	if (argc == 2) {
		if (strcmp(argv[1], "reset") == 0) {
			reset();
			return (TCL_OK);
		}
	}
	return (Agent::command(argc, argv));
}

// returns highest address assigned to a node
// (we cannot just use the number of nodes in the simulation, since it is possible to
//  manually assign an address at node creation)
int32_t RSVP_Agent::getHighestAssignedAddr() {
	// life would be so much easier if we could just access nn_ and nodelist_[] in C++;
	// unfortunately they are private so we have to retrieve them via OTcl :-(
	Tcl& tcl = Tcl::instance();
	tcl.evalf("[Simulator instance] get-highest-assigned-addr"); // handles both flat and hierarchical addressing
//	tcl.evalf("[expr [Node set nn_] - 1]"); // flat addressing only
	return atoi( tcl.result() );
}

int32_t RSVP_Agent::getNumberOfIfaces() {
	Tcl& tcl = Tcl::instance();
	tcl.evalf("NetworkInterface set ifacenum_");
	return atoi( tcl.result() );
}

// copies data of received packet to an INetworkBuffer and discards the packet
void RSVP_Agent::receivePacket( INetworkBuffer& buffer ) {
	assert(currentPkt);

	int userdatasize = ((PacketData*) currentPkt->userdata())->size(); // don't use datalen(): it will return wrong size!

//hdr_cmn* cmnh = hdr_cmn::access(currentPkt);
//printf("%2.4f|%i: %s::receivePacket(<msg: %s bufsize: %i>)\n",
//    Scheduler::instance().clock(), addr(), name(), packet_info.name(cmnh->ptype()), userdatasize );

	buffer.cloneFrom( currentPkt->accessdata(), userdatasize );

	// copy TTL from ns2's IP header to encapsulated IP header
	uint8  ttl = (uint8) hdr_ip::access(currentPkt)->ttl();
	uint8* buf = const_cast<uint8*>( buffer.getContents() );
	reinterpret_cast<struct __rsvp_ip4_header*>(buf)->ip_ttl = ttl;

	discardPacket();
}

// discard the last received packet
void RSVP_Agent::discardPacket() {
	Packet::free(currentPkt);
	currentPkt = NULL;
}

// sends content of <buffer>
void RSVP_Agent::sendPacket( uint8 msgType, const ONetworkBuffer& buffer, nsaddr_t destAddr, int32_t destPort ) {
	Packet* pkt = allocpkt( buffer.getUsedSize() );

	// set packet-type for display in nam and for filtering in rsvp-tab
	hdr_cmn* cmnh = hdr_cmn::access(pkt);
	switch( msgType ) {
	case Message::InitAPI:
		cmnh->ptype() = PT_RSVP_INIT_API;
		break;
	case Message::Path:
		cmnh->ptype() = PT_RSVP_PATH;
		break;
	case Message::Resv:
		cmnh->ptype() = PT_RSVP_RESV;
		break;
	case Message::PathErr:
		cmnh->ptype() = PT_RSVP_PATH_ERR;
		break;
	case Message::ResvErr:
		cmnh->ptype() = PT_RSVP_RESV_ERR;
		break;
	case Message::PathTear:
		cmnh->ptype() = PT_RSVP_PATH_TEAR;
		break;
	case Message::ResvTear:
		cmnh->ptype() = PT_RSVP_RESV_TEAR;
		break;
	case Message::ResvConf:
		cmnh->ptype() = PT_RSVP_RESV_CONF;
		break;
	case Message::Ack:
		cmnh->ptype() = PT_RSVP_ACK;
		break;
	case Message::Srefresh:
		cmnh->ptype() = PT_RSVP_SREFRESH;
		break;
	case Message::Load:
		cmnh->ptype() = PT_RSVP_LOAD;
		break;
	case Message::PathResv:
		cmnh->ptype() = PT_RSVP_PATH_RESV;
		break;
	default:
		cmnh->ptype() = PT_RSVP_UNKNOWN;
		break;
	}

	// set simulated packet size: used to calculate transmission delay; also nam refuses
	// to display our rsvp-packets if not set :-(
	cmnh->size() = buffer.getUsedSize(); // buffer already includes IPv4 header

	hdr_ip* iph = hdr_ip::access(pkt);
	iph->saddr() = getLocalAddr();
	iph->sport() = 0;
	iph->daddr() = destAddr;
	iph->dport() = destPort;

	memcpy( pkt->accessdata(), buffer.getContents(), buffer.getUsedSize() );

	// copy TTL from encapsulated IP header to ns2's IP header
	const uint8* buf = buffer.getContents();
	iph->ttl() = (int) reinterpret_cast<const PacketHeader*>(buf)->getTTL();

	// We need to decouple sending packets from receiving them, so that contexts of api
	// and daemon don't get mixed. Therefore we cannot use
	// 	send(pkt, 0);
	// Instead we schedule reception in 0.0 seconds, i.e. after we've left the daemon context.
	Scheduler::instance().schedule( target_, pkt, 0.0 );
}


// returns the outgoing interface towards <destNodeAddr>
char* RSVP_Agent::getUnicastRoute( nsaddr_t destNodeAddr ) {
	Tcl& tcl = Tcl::instance();
	tcl.evalf( "%s get-unicast-route %i", name(), destNodeAddr );
	return tcl.result();
}
