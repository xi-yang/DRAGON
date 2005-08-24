#ifndef rsvp_api_wrapper_h
#define rsvp_api_wrapper_h 1

#include "RSVP_API.h"
#include "RSVP_String.h"

#include "RSVP_Wrapper.h"

class NetAddress;
class INetworkBuffer;
class ONetworkBuffer;
class LogicalInterfaceUDP;
class CommandParser;

class RSVP_API_Agent;

union GenericUpcallParameter;
class UCPE;

class RSVP_API_Wrapper : public RSVP_Wrapper {
	RSVP_API*							rsvpApi;
	LogicalInterfaceUDP*	apiLif;
	CommandParser*				cp;
	bool									dummyEndFlag;

protected:
	virtual void setGlobalContext();
	virtual void clearGlobalContext();

public:
	RSVP_API_Wrapper( RSVP_API_Agent* rsvpApiAgent );
	virtual ~RSVP_API_Wrapper();

	void createAPI();
	void destroyAPI();

	virtual void notifyPacketArrival( int iif );
	void acceptCommand( int argc, const char*const* argv );
	void upcall( const GenericUpcallParameter& );
};
#endif
