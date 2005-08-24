#include "RSVP_Daemon_Wrapper.h"

#include "rsvp-daemon-agent.h"

#include "RSVP.h"
#include "RSVP_API_Server.h"
#include "RSVP_Global.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_LogicalInterfaceSet.h"
#include "RSVP_MessageProcessor.h"
#include "RSVP_Message.h"
#include "RSVP_BaseTimer.h"
#include "RSVP_TrafficControl.h"
#include "RSVP_SchedulerCBQ_NS2.h"

// hack for determining simulation start / when to compress interfaces
// ( more elegant alternative: use RSVP_Agent_Class::bind() and RSVP_Agent_CLass::method()
//  [see p.31 of manual], but then we need one more change to ns2-code )
static uint32 createCount = 0;
static uint32 daemonInstanceCount = 0;

void RSVP_Daemon_Wrapper::fireTimer() {
	setGlobalContext();
	checkTimer();
	clearGlobalContext();
}

void RSVP_Daemon_Wrapper::checkTimer() {
	setGlobalContext();
	static TimeValue remainingTime;
	if ( RSVP_Global::currentTimerSystem->executeTimer(remainingTime) ) {
		reinterpret_cast<RSVP_Daemon_Agent*>(rsvpAgent)->resched( remainingTime.getFractionalValue() );
	}
	clearGlobalContext();
}

RSVP_Daemon_Wrapper::RSVP_Daemon_Wrapper( RSVP_Agent* rsvpDaemonAgent )
	: RSVP_Wrapper( rsvpDaemonAgent ), rsvp(NULL), timerSystem(NULL),
	messageProcessor(NULL), apiPort(0), apiServer(NULL) {
	daemonInstanceCount += 1;
}

RSVP_Daemon_Wrapper::~RSVP_Daemon_Wrapper() {
	if ( reinterpret_cast<RSVP_Daemon_Agent*>(rsvpAgent)->status() == TIMER_PENDING ) {
		reinterpret_cast<RSVP_Daemon_Agent*>(rsvpAgent)->cancel();
	}
	destroyRSVPModule();
}

// methods for faking single-instance to rsvp-module
void RSVP_Daemon_Wrapper::setGlobalContext() {
	RSVP_Global::wrapper            = this;
	RSVP_Global::rsvp               = rsvp;
	RSVP_Global::currentTimerSystem = timerSystem;
	RSVP_Global::messageProcessor   = messageProcessor;
	RSVP::apiPort                   = apiPort;
	RSVP::apiServer                 = apiServer;
}

void RSVP_Daemon_Wrapper::clearGlobalContext() {
	RSVP::apiServer                 = NULL;
	RSVP::apiPort                   = 0;
	RSVP_Global::messageProcessor   = NULL;
	RSVP_Global::currentTimerSystem = NULL;
	RSVP_Global::rsvp               = NULL;
	RSVP_Global::wrapper            = 0;
}

void RSVP_Daemon_Wrapper::createRSVPModule() {
	LOG(1)( Log::NS, "RSVP_Daemon_Wrapper::createRSVPModule" );
	if (rsvp) destroyRSVPModule();

	apiPort         = rsvpAgent->getLocalPort();
	setGlobalContext();
	rsvp            = new RSVP( "", initialLifList );
	rsvp->setWrapper( this );
	timerSystem     = RSVP_Global::currentTimerSystem;
	messageProcessor= RSVP_Global::messageProcessor;
	apiServer       = RSVP::apiServer;
	clearGlobalContext();
	createCount += 1;
	if (createCount >= daemonInstanceCount) RSVP_Wrapper::compressInterfaces();
}

void RSVP_Daemon_Wrapper::destroyRSVPModule() {
	setGlobalContext();
	delete rsvp;
	rsvp             = NULL;
	timerSystem      = NULL;
	messageProcessor = NULL;
	apiPort          = 0;
	apiServer        = NULL;
	clearGlobalContext();
	createCount -= 1;
	if ( createCount == 0 ) RSVP_Wrapper::cleanup();
}

// notify RSVP module that a packet arrived at interface and is ready for pick-up
void RSVP_Daemon_Wrapper::notifyPacketArrival( int iif ) {
	if ( iif == UNKN_IFACE.value() ) iif = 0; // packets from local agents go to api-server
	const LogicalInterface* lif = rsvp->findInterfaceByAddress( iif );
	if ( lif ) {
		setGlobalContext();
		messageProcessor->readCurrentMessage( *lif );
		clearGlobalContext();
	} else {
		LOG(2)( Log::Packet, "Skipping packet from interface ", lif->getName() );
		rsvpAgent->discardPacket();
	}
	checkTimer();
}

const LogicalInterface* RSVP_Daemon_Wrapper::getUnicastRoute( const NetAddress& dest ) {
	String result = rsvpAgent->getUnicastRoute( mapInterfaceToNode( dest ) );
	if ( result == "api" ) {
		return RSVP::apiServer->getApiLif();
	} else if ( result == "" ) {
		return NULL;
	} else {
		return rsvp->findInterfaceByOif( result.chars() );
	}
}

const LogicalInterface* RSVP_Daemon_Wrapper::getMulticastRoute( const NetAddress& src, const NetAddress& dest, LogicalInterfaceSet& lifList ) {
	const LogicalInterface* inLif = NULL;
	// do unicast route lookup for now. need to change this later
	const LogicalInterface* outLif = getUnicastRoute( dest );
	if ( outLif != NULL ) {
		lifList.insert_unique( outLif );
	}
	return inLif;
}

void RSVP_Daemon_Wrapper::registerInterface( nsaddr_t nodeAddr, int iif, const String& oif,
        const String& linkObj, const String& linkType, ieee32float bandwidth, uint32 latency ) {

	static char buf[64];
	LOG(6)( Log::NS, "registering interface", iif, "at node", nodeAddr, "oif:", oif );
	sprintf( buf, "if%i_%i", nodeAddr, initialLifList.size() );
	LogicalInterface* lif = new LogicalInterface( buf, iif, 1500 );
	lif->setOif( oif );

	RSVP_Daemon_Agent* da = reinterpret_cast<RSVP_Daemon_Agent*>(rsvpAgent);
	TimeValue refreshInterval;
	refreshInterval.getFromFraction( da->getRefreshInterval() );
	lif->configureRefresh( refreshInterval );
#if defined(REFRESH_REDUCTION)
	lif->setRapidRefreshInterval( (uint32)(da->getRapidRefreshInterval()*1000.0) );
#endif

	TrafficControl* tc;
	if ( linkType == String("CBQLink") ) {
		tc = new TrafficControl( new SchedulerCBQ_NS2( linkObj, bandwidth, latency ) );
	} else {
		tc = new TrafficControl( NULL );
	}
	lif->configureTC( tc );

	initialLifList.push_back( lif );
	RSVP_Wrapper::interfaceList.push_back( RSVP_Wrapper::InterfaceNodeMap(lif, nodeAddr) );
	RSVP_Wrapper::interfaceCount += 1;
}

const LogicalInterface* RSVP_Daemon_Wrapper::lookupIfaceByOif( const String& oif ) {
	return rsvp->findInterfaceByOif( oif );
}
