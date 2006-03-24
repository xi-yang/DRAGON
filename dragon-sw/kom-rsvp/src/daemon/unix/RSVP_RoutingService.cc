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
#include "RSVP_IntServObjects.h"
#include "RSVP_RoutingService.h"
#include "RSVP.h"
#include "RSVP_Global.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_Log.h"
#include "RSVP_NetworkService.h"
#include "RSVP_NetworkServiceDaemon.h"
#include "RSVP_RSRR.h"
#if defined(NS2)
#include "RSVP_Daemon_Wrapper.h"
#endif
 #include <fcntl.h>
#if defined(SunOS) || defined(FreeBSD)
#include <sys/types.h>                           // div. typedefs
#include <sys/un.h>
#include <sys/socket.h>                          // socket
#include <net/route.h>                           // routing socket
#include <net/if_dl.h>                           // sockaddr_dl
#include <netinet/in.h>                          // sockaddr_in
static char routeBuffer[256];
#else
#include <asm/types.h>                           // div. typedefs
#include <sys/socket.h>                          // socket
#include <linux/netlink.h>                       // netlink socket
#include <linux/rtnetlink.h>                     // routing socket

struct RouteMessage {
	struct nlmsghdr hdr;
	struct rtmsg msg;
	char buffer[1024];
};
#endif

#if defined(REAL_NETWORK)
static int routingSocket;
#endif

static const uint16 ospf_rsvp_port = 2613;

class RoutingEntryList : public SimpleList<RoutingEntry*> {};

static pid_t my_pid = getpid();

ostream& operator<< (ostream& os, const RoutingEntry& rt ) {
	os << "dst: " << rt.dest << " mask: " << rt.mask;
	if (rt.gw != NetAddress(0)) os << " gw: " << rt.gw;
	os << " -> " << (rt.iface)->getName();
	return os;
}

void RoutingService::addRoute( const RoutingEntry& rte ) {
//@@hacked
//#if defined(VIRT_NETWORK)
	if( rte.iface ) {
		rtList->push_front( new RoutingEntry( rte ) );
	}
//#endif
}

RoutingService::RoutingService() : rsrr(NULL), rtList(new RoutingEntryList),
	mainPID(getpid()), queryCounter(0) {
#if defined(REAL_NETWORK)
#if defined(Linux)
	routingSocket = CHECK( socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE) );
	struct sockaddr_nl addr = { AF_NETLINK, 0, mainPID, 0 };
	CHECK( bind( routingSocket, (struct sockaddr*)&addr, sizeof(addr) ) );
#else
	routingSocket = CHECK( socket(PF_ROUTE, SOCK_RAW, PF_UNSPEC) );
#endif
	/* don't remove: routing daemon may send messages which block socket */
	NetworkServiceDaemon::registerRouting_Handle( routingSocket );
	rsrr = new RSRR();
#endif
	ospf_socket = 0;
}

RoutingService::~RoutingService() {
	RoutingEntryList::Iterator iter = rtList->begin();
	for ( ;iter != rtList->end(); ++iter ) {
		delete *iter;
	}
	delete rtList;
#if defined(REAL_NETWORK)
	delete rsrr;
	NetworkServiceDaemon::deregisterRouting_Handle( routingSocket );
	CHECK( close( routingSocket ) );
#endif
	if (ospf_socket){
		CHECK( close( ospf_socket ) );
		ospf_socket = 0;
	}
}

void RoutingService::init( LogicalInterfaceList& tmpLifList ) {
#if defined(REAL_NETWORK)
	if ( !rsrr->init( tmpLifList ) ) {
		delete rsrr;
		rsrr = NULL;
		LOG(1)( Log::Routing, "Routing: no mrouted found" );
	}
#endif
#if defined(VIRT_NETWORK)
	RoutingEntryList::Iterator iter = rtList->begin();
	for ( ;iter != rtList->end(); ++iter ) {
		LOG(2)( Log::Routing, "route: ", **iter );
	}
#endif
}

void RoutingService::init2() {
	RoutingEntryList::Iterator iter = rtList->begin();
	while ( iter != rtList->end() ) {
		if ( (*iter)->iface == NULL || (*iter)->iface->isDisabled() ) {
			delete *iter;
			iter = rtList->erase(iter);
		} else {
			LOG(2)( Log::Routing, "route: ", **iter );
			++iter;
		}
	}
}

#if defined(FreeBSD)
//non-blocking connect to OSPFD
bool RoutingService::ospf_socket_init (){
	  int val, ret;
	  struct sockaddr_in addr;
	  int flags, old_flags;
	  struct timeval tv;
	  fd_set sset;
	  socklen_t lon;

	  if (ospf_socket<=0)
		  ospf_socket = CHECK(socket (AF_INET, SOCK_STREAM, 0));
	  if (ospf_socket<0) 
	  	return false;

	  flags = old_flags = fcntl(ospf_socket, F_GETFL, 0);
/*
#if defined(O_NONBLOCK)
	    flags |= O_NONBLOCK;
#elif defined(O_NDELAY)
	    flags |= O_NDELAY;
#elif defined(FNDELAY)
	    flags |= FNDELAY;
#endif
*/

	  if (fcntl(ospf_socket, F_SETFL, flags) == -1) {
	      return false;
	  }
	  
	  /* Make server socket. */ 
	  memset (&addr, 0, sizeof (struct sockaddr_in));
	  addr.sin_family = AF_INET;
	  addr.sin_port = htons (ospf_rsvp_port);
#ifdef HAVE_SIN_LEN
	  addr.sin_len = sizeof(struct sockaddr_in);
#endif /* HAVE_SIN_LEN */
	  addr.sin_addr.s_addr = LogicalInterface::loopbackAddress.rawAddress();

	  ret = connect (ospf_socket, (struct sockaddr *) &addr, sizeof(addr));

	  if(ret < 0) {
	     if (errno == EINPROGRESS) { 
	         tv.tv_sec = 1;
	         tv.tv_usec = 0;
	         FD_ZERO(&sset);
	         FD_SET(ospf_socket, &sset);
	         if(select(ospf_socket+1, NULL, &sset, NULL, &tv) > 0) {
	             lon = sizeof(int);
	             getsockopt(ospf_socket, SOL_SOCKET, SO_ERROR, (void*)(&val), &lon); 
	             return ((val != 0)?false:true);
	         }
	         else 
	             return false;
	     }
	     else 
	         /*!EINPROGRESS */
	         return false;
	 }

	 if (fcntl(ospf_socket, F_SETFL, old_flags) == -1) 
	     return false;
	 else
	     return true;
}
#else
bool RoutingService::ospf_socket_init (){
  int ret;
  struct sockaddr_in addr;

  if (!ospf_socket){
		  ospf_socket = CHECK(socket (AF_INET, SOCK_STREAM, 0));
	
		  /* Make server socket. */
		  memset (&addr, 0, sizeof (struct sockaddr_in));
		  addr.sin_family = AF_INET;
		  addr.sin_port = htons (ospf_rsvp_port);
#ifdef HAVE_SIN_LEN
		  addr.sin_len = sizeof(struct sockaddr_in);
#endif /* HAVE_SIN_LEN */
		  addr.sin_addr.s_addr = LogicalInterface::loopbackAddress.rawAddress();
	
		  ret = connect(ospf_socket, (struct sockaddr *) &addr, sizeof(addr));
		  if (ret<0) return false;
  }
	
  return true;

}
#endif

void RoutingService::maskLength2IP (int masklen, NetAddress& netmask) const {
	uint8 *pnt;
	int bit;
	int offset;
	static uint8 maskbit[] = {0x00, 0x80, 0xc0, 0xe0, 0xf0,
						      0xf8, 0xfc, 0xfe, 0xff};
	in_addr mask;
	
	pnt = (uint8 *) &mask;

	offset = masklen / 8;
	bit = masklen % 8;
	  
	while (offset--)
	  *pnt++ = 0xff;

	if (bit)
	  *pnt = maskbit[bit];

	netmask = mask.s_addr;
	
}

void RoutingService::getPeerIPAddr(const NetAddress& myAddr, NetAddress& peerAddr) const {
	NetAddress mask(0);
	maskLength2IP (30, mask);
	NetAddress tmpAddr = myAddr.rawAddress() & mask.rawAddress();
	if (htonl(ntohl(tmpAddr.rawAddress())+1) == myAddr.rawAddress())
		peerAddr = htonl(ntohl(tmpAddr.rawAddress())+2);
	else
		peerAddr = htonl(ntohl(tmpAddr.rawAddress())+1);	
}


//Get explicit route from OSPF
//The explicit route starts from next hop (does not contains its own hop)
EXPLICIT_ROUTE_Object* RoutingService::getExplicitRouteByOSPF(const NetAddress& src, 
const NetAddress &dest, const SENDER_TSPEC_Object& sendTSpec, const LABEL_REQUEST_Object& labelReq)
{	
	//Write packet to OSPF socket ask for my hop control IP address
	uint8 message = GetExplicitRouteByOSPF;
	uint8 msgLength;
	if (sendTSpec.getService()==SENDER_TSPEC_Object::GMPLS_Sender_Tspec)
		//msgtype(8) + msglen(8) + service(8) + src(32) + dest IP (32) + switching(8) + encoding(8) + gpid(16) + bandwidth (32)
		msgLength = sizeof(uint8)*3 + src.size() + dest.size() + sizeof(uint32)*2;   
	else
		//msgtype(8) + msglen(8) + service(8) + src(32) + dest IP (32) + switching(8) + encoding(8) + gpid(16) + SonetTspec(4*32)
		msgLength = sizeof(uint8)*3 + src.size() + dest.size() + sizeof(uint32) + sizeof(uint32)*4;   
	ONetworkBuffer obuffer(msgLength);
	obuffer << msgLength << message << sendTSpec.getService() << src << dest;
	if (labelReq.getRequestedLabelType() == LABEL_Object::LABEL_GENERALIZED)
		obuffer << labelReq.getLspEncodingType() << labelReq.getSwitchingType() << labelReq.getGPid();
	else if (labelReq.getRequestedLabelType() == LABEL_Object::LABEL_MPLS)
		obuffer << labelReq.getL3Pid();
	else{
		LOG(1)(Log::MPLS, "MPLS: Waveband label not supported");
		return NULL;
	}
	if (sendTSpec.getService()==SENDER_TSPEC_Object::GMPLS_Sender_Tspec){
		obuffer << sendTSpec.get_p();
	}
	else{
		obuffer << sendTSpec.getSignalType() << sendTSpec.getRCC() ;
		obuffer << sendTSpec.getNCC() << sendTSpec.getNVC() << sendTSpec.getMT();
		obuffer << sendTSpec.getTransparency() << sendTSpec.getProfile();
	}
	CheckOspfSocket(write(ospf_socket, obuffer.getContents(), obuffer.getUsedSize()));

	msgLength = 0;
	//Read response from OSPF
	read(ospf_socket, (void *)&msgLength, sizeof(uint8));
	read(ospf_socket, (void *)&message, sizeof(uint8));

	if (msgLength <= sizeof(uint8)*2)
		return NULL;

	INetworkBuffer ibuffer(msgLength-sizeof(uint8));
	msgLength = read(ospf_socket, ibuffer.getWriteBuffer(), ibuffer.getSize());
	ibuffer.setWriteLength(msgLength);

	//Process response messages which contains IP address lists (ERO)
	NetAddress hop;
	EXPLICIT_ROUTE_Object *ero = new EXPLICIT_ROUTE_Object();
	while (ibuffer.getRemainingSize()){
		ibuffer >> hop;
       	ero->pushBack(AbstractNode(false, hop, (uint8)32));
	}
	return ero;

}


//Find control logical interface by data plane IP / interface ID
const LogicalInterface* RoutingService::findInterfaceByData( const NetAddress& ip, const uint32 ifID ) {
	//Write packet to OSPF socket ask for my hop control IP address
	uint8 message = FindInterfaceByData;
	uint8 msgLength = sizeof(uint8)*2+ip.size()+sizeof(uint32);
	ONetworkBuffer obuffer(msgLength);
	obuffer << msgLength << message << ip << ifID;
	CheckOspfSocket(write(ospf_socket, obuffer.getContents(), obuffer.getUsedSize()));

	msgLength = 0;
	//Read response from OSPF
	read(ospf_socket, (void *)&msgLength, sizeof(uint8));
	read(ospf_socket, (void *)&message, sizeof(uint8));

	if (msgLength <= sizeof(uint8)*2)
		return NULL;
	
	INetworkBuffer ibuffer(msgLength-sizeof(uint8));
	msgLength = read(ospf_socket, ibuffer.getWriteBuffer(), ibuffer.getSize());
	ibuffer.setWriteLength(msgLength);

	//Process response messages
	//Now myHop becomes my *control* IP address
	NetAddress myHop;
	ibuffer >> myHop;	

	return RSVP_Global::rsvp->findInterfaceByAddress(myHop);
}

//Find data plane IP / interface ID by control logical interface
bool RoutingService::findDataByInterface(const LogicalInterface& lif, NetAddress& ip, uint32& ifID) {
	uint8 message = FindDataByInterface;
	uint8 msgLength = sizeof(uint8)*2+lif.getAddress().size();
	ONetworkBuffer obuffer(msgLength);
	obuffer << msgLength << message << lif.getAddress();
	CheckOspfSocket(write(ospf_socket, obuffer.getContents(), obuffer.getUsedSize()));

	msgLength = 0;
	//Read response from OSPF
	read(ospf_socket, (void *)&msgLength, sizeof(uint8));
	read(ospf_socket, (void *)&message, sizeof(uint8));

	if (msgLength <= sizeof(uint8)*2)
			return false;

	INetworkBuffer ibuffer(msgLength-sizeof(uint8));
	msgLength = read(ospf_socket, ibuffer.getWriteBuffer(), ibuffer.getSize());
	ibuffer.setWriteLength(msgLength);
	uint32 aid;
	ibuffer >> ip >> aid;	
	if ((ifID >> 16) == 0)
		ifID = aid;
	return true;
}


//Find outgoing control logical interface by next hop data plane IP / interface ID
const LogicalInterface* RoutingService::findOutLifByOSPF( const NetAddress& nextHop, const uint32 ifID, NetAddress& gw   ) {
	uint8 message = FindOutLifByOSPF;
	uint8 msgLength = sizeof(uint8)*2+nextHop.size()+sizeof(uint32);
	ONetworkBuffer obuffer(msgLength);
	obuffer << msgLength << message << nextHop << ifID;
	CheckOspfSocket(write(ospf_socket, obuffer.getContents(), obuffer.getUsedSize()));

	msgLength = 0;
	//Read response from OSPF
	read(ospf_socket, (void *)&msgLength, sizeof(uint8));
	read(ospf_socket, (void *)&message, sizeof(uint8));

	//If OSPF is not able to resolve it, try looking up the static routing table, maybe there is one entry in it...
	if (msgLength <= sizeof(uint8)*2)
	{
	//@@@@ Static route resolution for interdomain links
		const LogicalInterface* lif = getUnicastRoute(nextHop, gw); 
		const NetAddress addr = NetAddress(gw.rawAddress());
		if (lif && RSVP_Global::rsvp->findInterfaceByAddress(addr))
			getPeerIPAddr(addr, gw);
			return lif;
		return RSVP_Global::rsvp->findInterfaceByAddress(nextHop);
	}

	INetworkBuffer ibuffer(msgLength-sizeof(uint8));
	msgLength = read(ospf_socket, ibuffer.getWriteBuffer(), ibuffer.getSize());
	ibuffer.setWriteLength(msgLength);
	
	//Process response messages
	//Now myHop becomes my *control* IP address
	NetAddress myHop;
	ibuffer >> myHop;	

	//Get next hop control IP address
	getPeerIPAddr(myHop, gw);

	return RSVP_Global::rsvp->findInterfaceByAddress(myHop);
}

//Get VLSR route
const void RoutingService::getVLSRRoutebyOSPF(const NetAddress& inRtID, const NetAddress& outRtID, const uint32 inIfId, const uint32 outIfId, VLSR_Route& vlsr) {
	uint8 message = GetVLSRRoutebyOSPF;
	uint8 msgLength = sizeof(uint8)*2+inRtID.size() + outRtID.size() + sizeof(uint32)*2;
	ONetworkBuffer obuffer(msgLength);
	obuffer << msgLength << message << inRtID << outRtID << inIfId << outIfId;
	CheckOspfSocket(write(ospf_socket, obuffer.getContents(), obuffer.getUsedSize()));

	msgLength = 0;
	//Read response from OSPF
	read(ospf_socket, (void *)&msgLength, sizeof(uint8));
	INetworkBuffer ibuffer(msgLength-sizeof(uint8));
	msgLength = read(ospf_socket, ibuffer.getWriteBuffer(), ibuffer.getSize());
	ibuffer.setWriteLength(msgLength);
	
	//Process response messages
	ibuffer >> message >> vlsr.switchID >> vlsr.inPort >> vlsr.outPort>>vlsr.vlanTag;

	return;

}

const void RoutingService::notifyOSPF(uint8 msgType, const NetAddress& ctrlIfIP, ieee32float bw  ) {
	//Write packet to OSPF socket ask for my hop control IP address
	if ((msgType == OspfResv || msgType == OspfPathTear || msgType == OspfResvTear) &&
		ospf_socket)
	{
		uint8 msgLength = sizeof(uint8)*2+ctrlIfIP.size()+sizeof(ieee32float);
		ONetworkBuffer obuffer(msgLength);
		obuffer << msgLength << msgType << ctrlIfIP << bw;
		CheckOspfSocket(write(ospf_socket, obuffer.getContents(), obuffer.getUsedSize()));
	}
}

//Hold or release bandwidth
const void RoutingService::holdBandwidthbyOSPF(u_int32_t port, float bw, bool hold) {
	uint8 message = HoldBandwidthbyOSPF;
	uint8 msgLength = sizeof(uint8)*2 + sizeof(uint32)*2 + sizeof(uint8);
	uint8 c_hold = hold ? 1 : 0;
	ONetworkBuffer obuffer(msgLength);
	obuffer << msgLength << message << port << bw <<c_hold;
	CheckOspfSocket(write(ospf_socket, obuffer.getContents(), obuffer.getUsedSize()));
}


//Hold or release VLAN Tag
const void RoutingService::holdVtagbyOSPF(u_int32_t vtag, bool hold) {
	uint8 message = HoldVtagbyOSPF;
	uint8 msgLength = sizeof(uint8)*2 + sizeof(uint32) + sizeof(uint8);
	uint8 c_hold = hold ? 1 : 0;
	ONetworkBuffer obuffer(msgLength);
	obuffer << msgLength << message << vtag <<c_hold;
	CheckOspfSocket(write(ospf_socket, obuffer.getContents(), obuffer.getUsedSize()));
}

// Get its loopback address
// used for filling the IP address field of the RSVP_HOP object
NetAddress RoutingService::getLoopbackAddress() {
	//for UNI_C, no OSPF service available
        if (RSVP_Global::rsvp->isUNI_C())
	        return LogicalInterface::loopbackAddress;

	//Write packet to OSPF socket ask for my hop control IP address
	uint8 message = GetLoopbackAddress;
	uint8 msgLength = sizeof(uint8)*2;
	ONetworkBuffer obuffer(msgLength);
	obuffer << msgLength << message;
	CheckOspfSocket(write(ospf_socket, obuffer.getContents(), obuffer.getUsedSize()));

	msgLength = 0;
	//Read response from OSPF
	read(ospf_socket, (void *)&msgLength, sizeof(uint8));
	read(ospf_socket, (void *)&message, sizeof(uint8));

	if (msgLength <= sizeof(uint8)*2)
	{
		return NetAddress(0);
	}
	else{
		INetworkBuffer ibuffer(msgLength-sizeof(uint8));
		msgLength = read(ospf_socket, ibuffer.getWriteBuffer(), ibuffer.getSize());
		ibuffer.setWriteLength(msgLength);
	
		NetAddress LoopBackAddr;
		ibuffer >> LoopBackAddr;	
		return LoopBackAddr;
	}
}


const LogicalInterface* RoutingService::getUnicastRoute( const NetAddress& dest, NetAddress& gw ) {
#if defined(REAL_NETWORK)
	NetAddress readDest = 0;
	if ( sendRouteRequest( dest ) ) {
		return getRouteReply( readDest, gw );
	}
#if defined(Linux)
	// Linux without netlink: return default interface (should be at LIH 1)
	return RSVP_Global::rsvp->findInterfaceByLIH(1);
#else
	return NULL;
#endif
#elif defined(VIRT_NETWORK)
	LogicalInterfaceSet lifList;
	getVirtualRoute( dest, lifList, gw );
	return lifList.empty() ? NULL : lifList.back();
#else // NS2
                                                assert( RSVP_Global::wrapper );
	gw = 0;
	return static_cast<RSVP_Daemon_Wrapper*>(RSVP_Global::wrapper)->getUnicastRoute( dest );
#endif
}

// lifList might already contain entries -> do not delete
// see Session::processPATH (API processing)
const LogicalInterface* RoutingService::getMulticastRoute( const NetAddress& src, const NetAddress& dest, LogicalInterfaceSet& lifList ) {
	const LogicalInterface* inLif = NULL;
#if defined(NS2)
                                                assert( RSVP_Global::wrapper );
	static_cast<RSVP_Daemon_Wrapper*>(RSVP_Global::wrapper)->getMulticastRoute( src, dest, lifList );
#endif
#if defined(REAL_NETWORK)
	if ( dest.isMulticast() && rsrr ) {
		rsrr->getRoute( src, dest, inLif, lifList );
	} else {
		NetAddress gw(0);
		const LogicalInterface* destLif = getUnicastRoute(dest,gw);
		if ( destLif ) {
			lifList.insert_unique( destLif );
		}
	}
#endif
#if defined(VIRT_NETWORK)
	NetAddress gw(0);
	getVirtualRoute( dest, lifList, gw );
#endif
	return inLif;
}

//virtual route
bool RoutingService::getRoute( const NetAddress& dest, LogicalInterface*& lif,  NetAddress& gw ) const {
	bool found = false;
	RoutingEntryList::ConstIterator iter = rtList->begin();
	for ( ; iter != rtList->end(); ++iter ) {
		if ( ((*iter)->mask & dest) == (*iter)->dest ) {
			lif = (LogicalInterface*)((*iter)->iface);
			gw = (*iter)->gw;
			found = true;
		}
	}
	return found;
}

void RoutingService::getVirtualRoute( const NetAddress& dest, LogicalInterfaceSet& lifList, NetAddress& gw ) const {
#if defined(VIRT_NETWORK)
	// only uses simple match and returns all matching routes (i.e. no longest-prefix match)
	RoutingEntryList::ConstIterator iter = rtList->begin();
	for ( ; iter != rtList->end(); ++iter ) {
		if ( ((*iter)->mask & dest) == (*iter)->dest ) {
			lifList.insert_unique( (*iter)->iface );
			gw = (*iter)->gw;
		}
	}
	LOG(S)( Log::Routing, "virtual route lookup for " ); LOG(C)( Log::Routing, dest ); LOG(C)( Log::Routing, " ->" );
	LogicalInterfaceSet::ConstIterator lifIter = lifList.begin();
	for ( ; lifIter != lifList.end(); ++lifIter ) {
		LOG(C)( Log::Routing, " " ); LOG(C)( Log::Routing, (*lifIter)->getName() );
	}
	LOG(C)( Log::Routing, endl );
#endif
}

const LogicalInterface* RoutingService::getAsyncUnicastRoutingEvent( NetAddress& dest, NetAddress& gateway ) {
	const LogicalInterface* outLif = NULL;
#if defined(REAL_NETWORK)
	LOG(1)( Log::Routing, "checking for asynchronous unicast routing info" );
	outLif = getRouteReply( dest, gateway, true );
#endif
	return outLif;
}

bool RoutingService::getAsyncMulticastRoutingEvent( NetAddress& src, NetAddress& dest, const LogicalInterface*& inLif, LogicalInterfaceSet& lifList ) {
#if defined(REAL_NETWORK)
	return rsrr->getAsyncRoutingEvent( src, dest, inLif, lifList );
#else
	return false;
#endif
}

#define WORD_BNDARY(x)  (((x)+sizeof(long)-1) & ~(sizeof(long)-1))
#define RTAX_DST        0
#define RTAX_GATEWAY    1
#define RTAX_NETMASK    2
#define RTAX_GENMASK    3
#define RTAX_IFP        4
#define RTAX_IFA        5
#define RTAX_AUTHOR     6
#define RTAX_BRD        7
#if !defined(RTAX_MAX)
#define RTAX_MAX        8
#endif
#if defined(FreeBSD)
#define RTA_NUMBITS			8 /* Number of bits used in RTA_* */
#define ROUNDUP(a, size) (((a) & ((size)-1)) ? (1 + ((a) | ((size)-1))) : (a))
#endif

bool RoutingService::sendRouteRequest( const NetAddress& dest ) const {
#if defined(REAL_NETWORK)
	LOG(2)( Log::Routing, "requesting unicast route for", dest );
#if defined(Linux)
	static RouteMessage routeMsg;
	initMemoryWithZero( &routeMsg, sizeof(routeMsg) );
	routeMsg.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	routeMsg.hdr.nlmsg_type = RTM_GETROUTE;
	routeMsg.hdr.nlmsg_flags = NLM_F_REQUEST;
	routeMsg.hdr.nlmsg_seq = ++queryCounter;
	routeMsg.hdr.nlmsg_pid = mainPID;
	routeMsg.msg.rtm_family = AF_INET;
	routeMsg.msg.rtm_dst_len = NetAddress::size() * 8;  // bit length is used here
	routeMsg.msg.rtm_src_len = 0;
	routeMsg.msg.rtm_tos = 0;
	routeMsg.msg.rtm_table = RT_TABLE_UNSPEC;
	routeMsg.msg.rtm_protocol = RTPROT_UNSPEC;
	routeMsg.msg.rtm_scope = RT_SCOPE_UNIVERSE;
	routeMsg.msg.rtm_type = RTN_UNSPEC;
	routeMsg.msg.rtm_flags = 0;

	struct rtattr *rta = (struct rtattr*)((char*)&routeMsg + NLMSG_ALIGN(routeMsg.hdr.nlmsg_len));
	rta->rta_type = RTA_DST;
	rta->rta_len = RTA_LENGTH( NetAddress::size() );
	uint32 addr = dest.rawAddress();
	copyMemory( RTA_DATA(rta), &addr, sizeof(addr) );
	routeMsg.hdr.nlmsg_len = NLMSG_ALIGN(routeMsg.hdr.nlmsg_len) + RTA_LENGTH( NetAddress::size() );

	if ( write(routingSocket, &routeMsg, routeMsg.hdr.nlmsg_len ) < 0 ) {
		ERROR(1)( Log::Error, "ERROR: sendmsg to netlink failed; netlink support in kernel?" );
		return false;
	}
#else
	initMemoryWithZero( routeBuffer, sizeof(routeBuffer) );
	struct rt_msghdr *mhp = (struct rt_msghdr *)routeBuffer;
	char* cp = routeBuffer + sizeof(rt_msghdr);
	struct sockaddr_in* s4 = (struct sockaddr_in *)cp;
	cp += WORD_BNDARY(sizeof(sockaddr_in));
	s4->sin_family = AF_INET;
	s4->sin_addr.s_addr = dest.rawAddress();
#if defined(HAVE_SIN_LEN)
	s4->sin_len = sizeof(sockaddr_in);
#endif
	struct sockaddr_dl* s5 = (struct sockaddr_dl *)cp;
	cp += WORD_BNDARY(sizeof(struct sockaddr_dl));
	s5->sdl_family = AF_LINK;
#if defined(HAVE_SIN_LEN)
	s5->sdl_len = sizeof(struct sockaddr_dl);
#endif
	mhp->rtm_version = RTM_VERSION;
	mhp->rtm_type = RTM_GET;
	mhp->rtm_seq = ++queryCounter;
	mhp->rtm_addrs = RTA_DST|RTA_IFP;
#if defined(FreeBSD)
	mhp->rtm_flags = RTF_UP|RTF_GATEWAY|RTF_HOST|RTF_STATIC;
#else /* SunOS */
	mhp->rtm_pid = my_pid;
#endif
	mhp->rtm_msglen = cp - routeBuffer;
	if ( write( routingSocket, routeBuffer, (size_t)mhp->rtm_msglen) < 0 ) {
		ERROR(2)( Log::Error, "ERROR: write failed, can't find route for", dest );
		return false;
	}
#endif // Linux vs. FreeBSD/Solaris
	return true;
#else
	return false;
#endif // REAL_NETWORK
}

const LogicalInterface*  RoutingService::getRouteReply( NetAddress& dest, NetAddress& gateway, bool async ) const {
#if defined(REAL_NETWORK)
	uint16 index = 0;
#if defined(Linux)
	static RouteMessage routeMsg;
	for (;;) {
		if ( read( routingSocket, &routeMsg, sizeof(routeMsg) ) < (int)(sizeof(struct nlmsghdr) + sizeof(struct rtmsg)) ) {
			ERROR(2)( Log::Error, "ERROR: read failed, can't find route for", dest );
	return NULL;
		}
		if ( (pid_t)routeMsg.hdr.nlmsg_pid == mainPID || (pid_t)routeMsg.hdr.nlmsg_pid == 0) {
		  if ( routeMsg.hdr.nlmsg_seq != queryCounter ) {
				// this is an async route notification -> should be buffered and delivered later
				ERROR(6)( Log::Error, "ERROR: async route reply seqno", routeMsg.hdr.nlmsg_seq, "type", (uint32)routeMsg.hdr.nlmsg_type, "but waiting for", queryCounter );
	  	} else {
	break;
			}
	  } else if ( async ) {
	  	return NULL;
	  } else {
			LOG(8)( Log::Routing, "WARNING: received route reply seqno", routeMsg.hdr.nlmsg_seq, "type", (uint32)routeMsg.hdr.nlmsg_type, "PID", (pid_t)routeMsg.hdr.nlmsg_pid, "but my PID is", mainPID );
	  }
	}
	int length = routeMsg.hdr.nlmsg_len - NLMSG_LENGTH(sizeof(rtmsg));
	static struct rtattr* rtResult[RTA_MAX+1];
	initMemoryWithZero( rtResult, sizeof(rtResult) );
	struct rtattr* rta = RTM_RTA( (rtmsg*)NLMSG_DATA(&routeMsg.hdr) );
	while (RTA_OK( rta, length)) {
		if (rta->rta_type <= RTA_MAX) rtResult[rta->rta_type] = rta;
		rta = RTA_NEXT(rta,length);
	}
	if ( rtResult[RTA_GATEWAY] ) {
		gateway = *(uint32*)RTA_DATA(rtResult[RTA_GATEWAY]);
	}
	if ( rtResult[RTA_DST] ) {
		dest = *(uint32*)RTA_DATA(rtResult[RTA_DST]);
	}
	if ( rtResult[RTA_OIF] ) {
		index = *(uint32*)RTA_DATA(rtResult[RTA_OIF]);
	} else {
		return NULL;
	}
#else
	struct rt_msghdr *mhp = (struct rt_msghdr *)routeBuffer;
	for (;;) {
		if ( read(routingSocket, routeBuffer, sizeof(routeBuffer)) < (int)sizeof(rt_msghdr) ) {
			ERROR(2)( Log::Error, "ERROR: read failed, can't find route for", dest );
	return NULL;
		}
		if ( mhp->rtm_pid == my_pid ) {
			if ( mhp->rtm_seq != queryCounter ) {
				// this is an async route notification -> should be buffered and delivered later
				ERROR(6)( Log::Error, "ERROR: async route reply seqno", mhp->rtm_seq, "type", (uint32)mhp->rtm_type, "but waiting for", queryCounter );
			} else {
	break;
			}
		} else if ( async ) {
			return NULL;
	  } else {
			LOG(8)( Log::Routing, "WARNING: received route reply seqno", mhp->rtm_seq, "type", (uint32)mhp->rtm_type, "PID", mhp->rtm_pid, "but my PID is", mainPID );
		}
	}
	caddr_t cp = (caddr_t)(mhp + 1);
	struct sockaddr* rti_info[RTA_NUMBITS];
	int i;
	for (i = 0; i < RTAX_MAX; i++) {
		if (mhp->rtm_addrs & (1 << i)) {
			struct sockaddr* sa = (struct sockaddr*)cp;
			rti_info[i] = sa;
#if defined(SunOS)
			if ( mhp->rtm_addrs & (1 << i) == RTA_IFP ) {
				cp += sizeof(struct sockaddr_dl);
			} else {
				cp += sizeof(struct sockaddr_in);
			}
#else
			cp = ((caddr_t)sa + (sa->sa_len ? ROUNDUP(sa->sa_len,sizeof(uint32)) : sizeof(uint32)));
#endif
		} else {
			rti_info[i] = NULL;
		}
	}
	struct sockaddr_in* s4;
	if ( rti_info[RTAX_GATEWAY] != NULL) {
		s4 = (struct sockaddr_in *)(rti_info[RTAX_GATEWAY]);
    if ( s4->sin_family == AF_INET ) {
			gateway = s4->sin_addr.s_addr;
		}
    if ( s4->sin_family == AF_LINK ){ //gre tunnel
    			   if (NetworkServiceDaemon::getInterfaceBySystemIndex( s4->sin_port))
	                        gateway = (NetworkServiceDaemon::getInterfaceBySystemIndex( s4->sin_port))->getAddress();
                }
 	}
	if (rti_info[RTAX_DST] != NULL) {
		s4 = (struct sockaddr_in *)(rti_info[RTAX_DST]);
    if ( s4->sin_family == AF_INET ) {
			dest = s4->sin_addr.s_addr;
		}
	}
	struct sockaddr_dl* sdlp = NULL;
	if (rti_info[RTAX_IFP] != NULL) {
		sdlp = (struct sockaddr_dl *)(rti_info[RTAX_IFP]);
									assert( sdlp->sdl_family == AF_LINK && sdlp->sdl_nlen > 0 );
		index = sdlp->sdl_index;
	} else {
		return NULL;
	}
#endif // Linux vs. FreeBSD/Solaris
	LOG(6)( Log::Routing, "unicast route lookup result: index ", index, "via", gateway, "reported dest is", dest );
	return NetworkServiceDaemon::getInterfaceBySystemIndex( index );
#else
	return NULL;
#endif // REAL_NETWORK
}

#if defined(Linux) && defined(REAL_NETWORK)
// needed for MPLS ingress -> route entry points to MPLS forwarding entry
void RoutingService::doRouteModification( bool add, const NetAddress& dest, const LogicalInterface* lif, const NetAddress& gw, uint32 handle ) {
	static RouteMessage routeMsg;
	initMemoryWithZero( &routeMsg, sizeof(routeMsg) );
	routeMsg.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	if ( add ) {
		routeMsg.hdr.nlmsg_type = RTM_NEWROUTE;
		routeMsg.hdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL;
	} else {
		routeMsg.hdr.nlmsg_type = RTM_DELROUTE;
		routeMsg.hdr.nlmsg_flags = NLM_F_REQUEST;
	}
	routeMsg.hdr.nlmsg_seq = ++queryCounter;
	routeMsg.hdr.nlmsg_pid = mainPID;
	routeMsg.msg.rtm_family = AF_INET;
	routeMsg.msg.rtm_dst_len = NetAddress::size() * 8;  // bit length is used here
	routeMsg.msg.rtm_src_len = 0;
	routeMsg.msg.rtm_tos = 0;
	routeMsg.msg.rtm_table = RT_TABLE_MAIN;
	routeMsg.msg.rtm_protocol = RTPROT_STATIC;
	if ( gw == NetAddress(0) ) {
		routeMsg.msg.rtm_scope = RT_SCOPE_LINK;
	} else {
		routeMsg.msg.rtm_scope = RT_SCOPE_UNIVERSE;
	}
	routeMsg.msg.rtm_type = RTN_UNICAST;
	routeMsg.msg.rtm_flags = 0;

	struct rtattr *rta = (struct rtattr*)((char*)&routeMsg + NLMSG_ALIGN(routeMsg.hdr.nlmsg_len));
	rta->rta_type = RTA_DST;
	rta->rta_len = RTA_LENGTH( NetAddress::size() );
	uint32 addr = dest.rawAddress();
	copyMemory( RTA_DATA(rta), &addr, sizeof(addr) );
	routeMsg.hdr.nlmsg_len = NLMSG_ALIGN(routeMsg.hdr.nlmsg_len) + rta->rta_len;

	rta = (struct rtattr*)((char*)&routeMsg + NLMSG_ALIGN(routeMsg.hdr.nlmsg_len));
	uint32 data = NetworkServiceDaemon::getLoopbackInterfaceIndex();
	if ( lif ) data = lif->getSysIndex();
	rta->rta_type = RTA_OIF;
	rta->rta_len = RTA_LENGTH( sizeof(data) );
	copyMemory( RTA_DATA(rta), &data, sizeof(data) );
	routeMsg.hdr.nlmsg_len = NLMSG_ALIGN(routeMsg.hdr.nlmsg_len) + rta->rta_len;

	if ( gw != NetAddress(0) ) {
		rta = (struct rtattr*)((char*)&routeMsg + NLMSG_ALIGN(routeMsg.hdr.nlmsg_len));
		rta->rta_type = RTA_GATEWAY;
		rta->rta_len = RTA_LENGTH( NetAddress::size() );
		addr = gw.rawAddress();
		copyMemory( RTA_DATA(rta), &addr, sizeof(addr) );
		routeMsg.hdr.nlmsg_len = NLMSG_ALIGN(routeMsg.hdr.nlmsg_len) + rta->rta_len;
	}

	if ( handle != 0 ) {
		rta = (struct rtattr*)((char*)&routeMsg + NLMSG_ALIGN(routeMsg.hdr.nlmsg_len));
		rta->rta_type = RTA_FLOW;
		rta->rta_len = RTA_LENGTH( sizeof(handle) );
		copyMemory( RTA_DATA(rta), &handle, sizeof(handle) );
		routeMsg.hdr.nlmsg_len = NLMSG_ALIGN(routeMsg.hdr.nlmsg_len) + rta->rta_len;
	}

	if ( write(routingSocket, &routeMsg, routeMsg.hdr.nlmsg_len ) < 0 ) {
		ERROR(2)( Log::Error, "ERROR: sendmsg to netlink failed, cannot complete route modification for", dest );
	}
}
#endif /* Linux */

