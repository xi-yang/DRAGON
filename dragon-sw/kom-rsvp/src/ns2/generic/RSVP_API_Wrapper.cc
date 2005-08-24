#include "RSVP_API_Wrapper.h"

#include "rsvp-api-agent.h"

#include "RSVP_API.h"
#include "RSVP_API_Upcall.h"
#include "RSVP_Global.h"
#include "CommandParser.h"

#include <iosfwd>
#if defined(HAVE_SSTREAM)
#include <sstream>
#else
#include <strstream>
#define ostringstream ostrstream 
#define istringstream istrstream
#endif

RSVP_API_Wrapper::RSVP_API_Wrapper( RSVP_API_Agent* rsvpApiAgent )
	: RSVP_Wrapper(rsvpApiAgent), rsvpApi(NULL), apiLif(NULL), cp(NULL),
	dummyEndFlag(false) {}

RSVP_API_Wrapper::~RSVP_API_Wrapper() {
	destroyAPI();
}

// methods for faking single-instance to api
void RSVP_API_Wrapper::setGlobalContext() {
	RSVP_Global::wrapper = this;
	RSVP_API::apiLif     = apiLif;
}

void RSVP_API_Wrapper::clearGlobalContext() {
	RSVP_API::apiLif     = NULL;
	RSVP_Global::wrapper = NULL;
}

void RSVP_API_Wrapper::createAPI() {
	LOG(1)( Log::NS, "RSVP_API_Wrapper::createAPI()" );
	if (rsvpApi) destroyAPI();
	if (cp) delete cp;
	setGlobalContext();
	rsvpApi = new RSVP_API( (uint16)reinterpret_cast<RSVP_API_Agent*>(rsvpAgent)->getRsvpDaemonPort() );
	apiLif  = RSVP_API::apiLif;
	cp = new CommandParser(*this,*rsvpApi,dummyEndFlag);
	clearGlobalContext();
}

void RSVP_API_Wrapper::destroyAPI() {
	setGlobalContext();
	delete rsvpApi;
	rsvpApi = NULL;
	delete cp;
	cp = NULL;
	clearGlobalContext();
}

// notify RSVP module that a packet arrived at interface and is ready for pick-up
void RSVP_API_Wrapper::notifyPacketArrival( int iif ) {
	LOG(2)( Log::NS, "RSVP_API_Wrapper::notifyPacketArrrival at", iif );
	setGlobalContext();
	rsvpApi->receiveAndProcess();
	clearGlobalContext();
}

void RSVP_API_Wrapper::acceptCommand( int argc, const char*const* argv ) {
	String commandString;
	for ( int i = 1; i < argc; i++ ) {
		commandString += String(argv[i]);
		if ( i < argc-1 ) commandString += " ";
	}
	std::istringstream is(commandString.chars());
	setGlobalContext();
	cp->setIS( is );
	cp->execNextCommand();
	clearGlobalContext();
}

void RSVP_API_Wrapper::upcall( const GenericUpcallParameter& upcallPara ) {
	std::ostringstream os;
	os << rsvpAgent->name();

	switch( upcallPara.generalInfo->infoType ) {
		case UpcallParameter::PATH_EVENT:
			os << " upcall PATH " << *upcallPara.pathEvent;
			break;
		case UpcallParameter::RESV_EVENT:
			os << " upcall RESV " << *upcallPara.resvEvent;
			break;
		case UpcallParameter::PATH_TEAR:
			os << " upcall PATH_TEAR " << *upcallPara.pathTear;
			break;
		case UpcallParameter::RESV_TEAR:
			os << " upcall RESV_TEAR " << *upcallPara.resvTear;
			break;
		case UpcallParameter::PATH_ERROR:
			os << " upcall PATH_ERROR " << *upcallPara.pathError;
			break;
		case UpcallParameter::RESV_ERROR:
			os << " upcall RESV_ERROR " << *upcallPara.resvError;
			break;
		case UpcallParameter::RESV_CONFIRM:
			os << " upcall RESV_CONFIRM " << *upcallPara.resvConfirm;
			break;
		default:
			os << "puts \"INTERNAL ERROR: API upcall with unknown info type\"";
			break;
	}

	// substitute "[" and "]" for "\[" and "\]", respectively, so that OTcl does not try to interpret it
#if defined(HAVE_SSTREAM)
	String buffer( os.str().c_str() );
#else
	String buffer( os.str() );
#endif
	String upcallCommand;
	uint32 i = 0;
	for ( ; i < buffer.length(); i += 1 ) {
		switch (buffer[i]) {
			case '[': upcallCommand += "\\["; break;
			case ']': upcallCommand += "\\]"; break;
			default: upcallCommand += buffer[i]; break;
		}
	}
	reinterpret_cast<RSVP_API_Agent*>(rsvpAgent)->upcall( upcallCommand.chars() );
}
