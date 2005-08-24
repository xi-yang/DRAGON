#ifndef rsvp_wrapper_h
#define rsvp_wrapper_h 1

#include "RSVP_BasicTypes.h"
#include "RSVP_List.h"

typedef int32_t nsaddr_t; // from config.h

class RSVP_Agent;
class INetworkBuffer;
class ONetworkBuffer;
class LogicalInterface;
class LogicalInterfaceSet;

class RSVP_Wrapper {
protected:
	RSVP_Agent* rsvpAgent;
	virtual void setGlobalContext() = 0;
	virtual void clearGlobalContext() = 0;

	static const LogicalInterface** nodeToInterfaceMap;
	static uint32 nodeCount;
	static int* interfaceToNodeMap;
	static uint32 interfaceCount;

	struct InterfaceNodeMap {
		const LogicalInterface* lif;
		nsaddr_t node;
		InterfaceNodeMap( const LogicalInterface* l = NULL, nsaddr_t n = -1 )
			: lif(l), node(n) {}
	};
	static SimpleList<InterfaceNodeMap> interfaceList;

public:
	RSVP_Wrapper( RSVP_Agent* rsvpAgent );
	virtual ~RSVP_Wrapper() {}

	nsaddr_t getNodeAddr();
	uint16 getLocalPort();

	virtual void notifyPacketArrival( int iif ) = 0;
	void receivePacket( INetworkBuffer& buffer );
	void sendPacket( const ONetworkBuffer& buffer, const NetAddress& dest, uint16 destPort );

	static nsaddr_t mapInterfaceToNode( const NetAddress& interface ) {
		if (interfaceToNodeMap && interface.rawAddress() < interfaceCount)
			return interfaceToNodeMap[interface.rawAddress()];
		else return -1;
	}
	static const LogicalInterface* mapNodeToInterface( const NetAddress& node ) {
		assert(nodeToInterfaceMap); // if this assertion fails compressInterfaces() has not been called; usually happens when OTcl can't
		                            // execute a class method (misspelled? check <otclclass>::method()!) and creates an invisible object :-(
		if ( nodeToInterfaceMap[node.rawAddress()] == NULL ) {
			cerr << "RSVP_Wrapper::mapNodeToInterface(): Node " << node.rawAddress();
			cerr << " does not exist or is not an RSVP enabled node!" << endl;
			exit(1);
		}
		return nodeToInterfaceMap[node.rawAddress()];
	}
	static void compressInterfaces();
	static void cleanup();

};
#endif
