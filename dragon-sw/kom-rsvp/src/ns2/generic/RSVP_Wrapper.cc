#include "RSVP_Wrapper.h"

#include "rsvp-agent.h"
#include "random.h"

#include "RSVP_Lists.h"
#include "RSVP_Log.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_Global.h"
//#include "RSVP_Global.h"
#include "RSVP_PacketHeader.h"
#include "RSVP_TimeValue.h"

RSVP_Wrapper::RSVP_Wrapper( RSVP_Agent* rsvpAgent ) : rsvpAgent(rsvpAgent) {}

const LogicalInterface** RSVP_Wrapper::nodeToInterfaceMap = NULL;
uint32 RSVP_Wrapper::nodeCount = 0;
int* RSVP_Wrapper::interfaceToNodeMap = NULL;
uint32 RSVP_Wrapper::interfaceCount = 0;
SimpleList<RSVP_Wrapper::InterfaceNodeMap> RSVP_Wrapper::interfaceList;

nsaddr_t RSVP_Wrapper::getNodeAddr() {
	return rsvpAgent->getLocalAddr();
}

uint16 RSVP_Wrapper::getLocalPort() {
	return( rsvpAgent->getLocalPort() );
}

// pick-up the received packet
void RSVP_Wrapper::receivePacket( INetworkBuffer& buffer ) {
	LOG(1)( Log::NS, "RSVP_Wrapper::receivePacket" );
	rsvpAgent->receivePacket( buffer );
}

// sends a packet encapsulated in an ns2-IP-packet
void RSVP_Wrapper::sendPacket( const ONetworkBuffer& buffer, const NetAddress& dest, uint16 destPort ) {
	LOG(4)( Log::NS, "RSVP_Wrapper::sendPacket to node", dest.rawAddress(), "port", destPort );
	const uint8* buf = buffer.getContents();
	uint8 msgType = buf[reinterpret_cast<const PacketHeader*>(buf)->getHeaderLength() + 1];
	rsvpAgent->sendPacket( msgType, buffer, dest.rawAddress(), destPort );
}

// creates mapping between node addresses and interface addresses
void RSVP_Wrapper::compressInterfaces() {
	nodeCount  = RSVP_Agent::getHighestAssignedAddr() + 1;
	interfaceCount = RSVP_Agent::getNumberOfIfaces() + 1;
	nodeToInterfaceMap = new const LogicalInterface*[nodeCount];
	initMemoryWithZero( nodeToInterfaceMap, nodeCount*sizeof(LogicalInterface*) );
	interfaceToNodeMap = new int[interfaceCount];
	initMemoryWithZero( interfaceToNodeMap, (interfaceCount)*sizeof(int) );
	SimpleList<RSVP_Wrapper::InterfaceNodeMap>::ConstIterator iter = interfaceList.begin();
	while ( iter != interfaceList.end() ) {
		if (nodeToInterfaceMap[(*iter).node] == NULL) {
			nodeToInterfaceMap[(*iter).node] = (*iter).lif;
			LOG(6)( Log::NS, "node", (*iter).node, "->", (*iter).lif->getName(), "/", (*iter).lif->getAddress() );
		}
		interfaceToNodeMap[(*iter).lif->getAddress().rawAddress()] = (*iter).node;
		LOG(5)( Log::NS, (*iter).lif->getName(), "/", (*iter).lif->getAddress(), "-> node", (*iter).node );
		iter = interfaceList.erase( iter );
	}
}

void RSVP_Wrapper::cleanup() {
	delete [] nodeToInterfaceMap;
	delete [] interfaceToNodeMap;
}

void getSimulatorTime( timerep& t ) {
	if (!&Scheduler::instance()) {
		t.tv_sec  = 0;
		t.tv_usec = 0;
	} else {
		double clock = Scheduler::instance().clock();
		sint64 usec = (sint64) (1000000.0 * clock);
		t.tv_sec  = ( usec / 1000000 );
		t.tv_usec = ( usec % 1000000 );
	}
}

int getRandomNumber() {
	return Random::random();
}

int getCurrentNodeNumber() {
	if (RSVP_Global::wrapper) return RSVP_Global::wrapper->getNodeAddr();
	else return -1;
}

int getNodeFromIface( const NetAddress& addr ) {
	return RSVP_Wrapper::mapInterfaceToNode(addr);
}
