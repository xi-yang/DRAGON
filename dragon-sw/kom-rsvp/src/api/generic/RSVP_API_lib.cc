#include "RSVP_API.h"
#include "RSVP_API_StateBlock.h"
#include "RSVP_API_Upcall.h"
#include "RSVP_FlowDescriptor.h"
#include "RSVP_Global.h"
#include "RSVP_IntServObjects.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_Message.h"
#include "RSVP_NetworkService.h"
#include "RSVP_PacketHeader.h"

#if !defined(NS2)
// trick linker
void LogicalInterface::init( uint32 ) { assert(0); }
const LogicalInterface* LogicalInterface::receiveBuffer( INetworkBuffer&, PacketHeader& ) const { assert(0); return NULL; }
void LogicalInterface::sendBuffer( const ONetworkBuffer&, const NetAddress&, const NetAddress& ) const { assert(0); }
class TrafficControl { public: ~TrafficControl(); };
TrafficControl::~TrafficControl() {}
ostream& operator<< ( ostream& os, const TrafficControl& tc ) { return os; }
#endif

