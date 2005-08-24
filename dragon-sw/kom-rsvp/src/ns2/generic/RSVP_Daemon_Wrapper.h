#ifndef rsvp_daemon_wrapper_h
#define rsvp_daemon_wrapper_h 1

#include "RSVP_Lists.h"
#include "RSVP_Wrapper.h"

class RSVP;
class MessageProcessor;
class NetAddress;
class INetworkBuffer;
class ONetworkBuffer;
class LogicalInterfaceSet;
class API_Server;
class TimerSystem;

class RSVP_Daemon_Wrapper : public RSVP_Wrapper {
	RSVP*              rsvp;
	TimerSystem*       timerSystem;
	MessageProcessor*  messageProcessor;
	uint16             apiPort;
	API_Server*        apiServer;

	LogicalInterfaceList initialLifList;

protected:
	virtual void setGlobalContext();
	virtual void clearGlobalContext();

public:
	RSVP_Daemon_Wrapper( RSVP_Agent* rsvpDaemonAgent );
	virtual ~RSVP_Daemon_Wrapper();

	void createRSVPModule();
	void destroyRSVPModule();

	virtual void notifyPacketArrival( int iif );

	const LogicalInterface* getUnicastRoute( const NetAddress& dest );
	const LogicalInterface* getMulticastRoute( const NetAddress& src, const NetAddress& dest, LogicalInterfaceSet& lifList );

	void registerInterface( nsaddr_t nodeAddr, int iif, const String& oif, const String& linkObj,
	        const String& linkType, ieee32float totalBw, uint32 latency );
	const LogicalInterface* lookupIfaceByOif( const String& oif );

	void checkTimer();
	void fireTimer();
};
#endif
