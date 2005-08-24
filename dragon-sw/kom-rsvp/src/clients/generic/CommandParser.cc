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
#include "CommandParser.h"
#include "RSVP_API.h"
#include "RSVP_API_Upcall.h"
#include "RSVP_NetworkService.h"
#include "RSVP_String.h"
#if defined(NS2)
#include "RSVP_LogicalInterface.h"
#include "RSVP_API_Wrapper.h"
#endif

#include <iostream>

FLOWSPEC_Object* CommandParser::readFlowspec( bool guaranteedService ) {
	TSpec tspec;
	RSpec rspec( 0, 0 );
	FLOWSPEC_Object* flowspec;
	*is >> tspec;
	if ( guaranteedService ) {
		*is >> rspec;
		flowspec = new FLOWSPEC_Object( tspec, rspec );
	} else {
		flowspec = new FLOWSPEC_Object( tspec );
	}
	return flowspec;
}

void CommandParser::upcall( const GenericUpcallParameter& upcallPara, CommandParser* This ) {
	if ( upcallPara.generalInfo->infoType == UpcallParameter::PATH_EVENT ) {
		if ( This->waitpathSender == FILTER_SPEC_Object::anyFilter
			|| This->waitpathSender == upcallPara.pathEvent->senderTemplate ) {
			This->gotPath = true;
		}
	}
	LOG(1)( Log::Parser, "***** UPCALL *****" );
	switch( upcallPara.generalInfo->infoType ) {
		case UpcallParameter::PATH_EVENT:
			LOG(2)( Log::Parser, "PATH_EVENT:", *upcallPara.pathEvent );
			This->lastPathSender = upcallPara.pathEvent->senderTemplate;
			break;
		case UpcallParameter::RESV_EVENT:
			LOG(2)( Log::Parser, "RESV_EVENT:", *upcallPara.resvEvent );
			break;
		case UpcallParameter::PATH_TEAR:
			LOG(2)( Log::Parser, "PATH_TEAR:", *upcallPara.pathTear );
			break;
		case UpcallParameter::RESV_TEAR:
			LOG(2)( Log::Parser, "RESV_TEAR:", *upcallPara.resvTear );
			break;
		case UpcallParameter::PATH_ERROR:
			LOG(2)( Log::Parser, "PATH_ERROR:", *upcallPara.pathError );
			break;
		case UpcallParameter::RESV_ERROR:
			LOG(2)( Log::Parser, "RESV_ERROR:", *upcallPara.resvError );
			break;
		case UpcallParameter::RESV_CONFIRM:
			LOG(2)( Log::Parser, "RESV_CONFIRM:", *upcallPara.resvConfirm );
			break;
		default:
 			ERROR(1)( Log::Error, "INTERNAL ERROR: API upcall with unknown info type" );
			break;
	}
#if defined(NS2)
	This->wrapper.upcall( upcallPara );
#endif
}

void CommandParser::sessionCommand() {
	NetAddress addr;
	uint16 port, proto;
	*is >> addr >> port >> proto;
#if defined(NS2)
	addr = RSVP_Wrapper::mapNodeToInterface(addr)->getAddress();
#endif
	LOG(4)( Log::Parser, "CP session:", addr, proto, port );
	if ( addr.isMulticast() ) {
#if !defined(NS2)
		dummyFD = socket( AF_INET, SOCK_DGRAM, 0 );
		NetworkService::joinMCastGroupIP4( dummyFD, addr );
#endif
	}
	currentSession = api.createSession( addr, proto, port, (UpcallProcedure)upcall, this );
}

void CommandParser::senderCommand() {
	uint16 port;
	TSpec tspec;
	*is >> port >> tspec;
	LOG(3)( Log::Parser, "CP sender:", port, tspec );
	//api.createSender( currentSession, port, tspec, 127, NULL, NULL );
}

void CommandParser::unsenderCommand() {
	uint16 port;
	*is >> port;
	LOG(2)( Log::Parser, "CP unsender:", port );
	api.releaseSender( currentSession, port, 127 );
}

#if defined(ONEPASS_RESERVATION)
void CommandParser::senderResvCommand() {
	uint16 port;
	uint16 rsp = 0, srp = 0;
	TSpec tspec;
	*is >> port >> tspec;
	readNextCommand();
	if ( token == "duplex" ) {
		*is >> srp >> rsp;
		readNextCommand();
	}
	LOG(3)( Log::Parser, "CP senderresv:", port, tspec );
//	api.createSender( currentSession, port, tspec, 127, NULL, NULL, true, rsp, srp );
}
#endif

void CommandParser::reserveCommand() {
	String filterString;
	FilterStyle style;
	FlowDescriptorList fdList;
	*is >> filterString;
	if ( filterString == "wf" ) {
		style = WF;
	} else if ( filterString == "ff" ) {
		style = FF;
	} else if ( filterString == "se" ) {
		style = SE;
	} else {
		ERROR(2)( Log::Error, "ERROR in command file, unknown filter style:", filterString );
		while ( !readNextCommand() );
		return;
	}
	bool guaranteedService = true;
	String serviceClassString;
	*is >> serviceClassString;
	if ( serviceClassString == "cl" ) {
		guaranteedService = false;
	} else if ( serviceClassString == "g" ) {
		guaranteedService = true;
	} else {
		ERROR(2)( Log::Error, "ERROR in command file, unknown service class:", serviceClassString );
		while ( !readNextCommand() );
		return;
	}
	NetAddress addr;
	uint16 port;
	FLOWSPEC_Object* flowspec = NULL;
	switch (style) {
	case WF:
		fdList.push_back( FlowDescriptor() );
		flowspec = readFlowspec( guaranteedService );
		fdList.back().setFlowspec( flowspec );
		flowspec->destroy();
		readNextCommand();
		break;
	case FF:
		while ( !readNextCommand() ) {
			fdList.push_back( FlowDescriptor() );
			*is >> port;
#if defined(NS2)
			if ( token == "-1" ) {
				addr = lastPathSender.getSrcAddress();
				port = lastPathSender.getLspId();
			} else {
				addr = RSVP_Wrapper::mapNodeToInterface(token)->getAddress();
			}
#else
			addr = NetAddress( token );
#endif
			fdList.back().filterSpecList.push_back( FILTER_SPEC_Object( addr, port ) );
			LOG(3)( Log::Parser, "adding sender: ", addr, port );
			flowspec = readFlowspec( guaranteedService );
			fdList.back().setFlowspec( flowspec );
			flowspec->destroy();
		}
		break;
	case SE:
		fdList.push_back( FlowDescriptor() );
		flowspec = readFlowspec( guaranteedService );
		fdList.back().setFlowspec( flowspec );
		flowspec->destroy();
		while ( !readNextCommand() ) {
			*is >> port;
#if defined(NS2)
			if ( token == "-1" ) {
				addr = lastPathSender.getSrcAddress();
				port = lastPathSender.getLspId();
			} else {
				addr = RSVP_Wrapper::mapNodeToInterface(token)->getAddress();
			}
#else
			addr = NetAddress( token );
#endif
			fdList.back().filterSpecList.push_back( FILTER_SPEC_Object( addr, port ) );
			LOG(3)( Log::Parser, "adding sender: ", addr, port );
		}
		break;
	default:
		FATAL(1)( Log::Fatal, "FATAL INTERNAL ERROR in CommandParser." );
		abortProcess();
	};
	LOG(3)( Log::Parser, "CP reserve:", STYLE_Object(style),fdList.back() );
	POLICY_DATA_Object* po = NULL;
	if ( ucpe ) {
		po = new POLICY_DATA_Object;
		po->addUCPE( ucpe );
	}
	api.createReservation( currentSession, true, style, fdList, po );
	if (po) po->destroy();
	ucpe = NULL;
}

void CommandParser::unreserveCommand() {
	String filterString;
	FilterStyle style;
	FlowDescriptorList fdList;
	*is >> filterString;
	if ( filterString == "wf" ) {
		style = WF;
	} else if ( filterString == "ff" ) {
		style = FF;
	} else if ( filterString == "se" ) {
		style = SE;
	} else {
		ERROR(2)( Log::Error, "ERROR in command file, unknown filter style:", filterString );
		while ( !readNextCommand() );
		return;
	}
	NetAddress addr;
	uint16 port;
	switch (style) {
	case WF:
		fdList.push_back( FlowDescriptor() );
		readNextCommand();
		break;
	case FF:
		while ( !readNextCommand() ) {
			fdList.push_back( FlowDescriptor() );
			*is >> port;
#if defined(NS2)
			if ( token == "-1" ) {
				addr = lastPathSender.getSrcAddress();
				port = lastPathSender.getLspId();
			} else {
				addr = RSVP_Wrapper::mapNodeToInterface(token)->getAddress();
			}
#else
			addr = NetAddress( token );
#endif
			fdList.back().filterSpecList.push_back( FILTER_SPEC_Object( addr, port ) );
			LOG(3)( Log::Parser, "adding sender: ", addr, port );
		}
		break;
	case SE:
		fdList.push_back( FlowDescriptor() );
		while ( !readNextCommand() ) {
			*is >> port;
#if defined(NS2)
			if ( token == "-1" ) {
				addr = lastPathSender.getSrcAddress();
				port = lastPathSender.getLspId();
			} else {
				addr = RSVP_Wrapper::mapNodeToInterface(token)->getAddress();
			}
#else
			addr = NetAddress( token );
#endif
			fdList.back().filterSpecList.push_back( FILTER_SPEC_Object( addr, port ) );
			LOG(3)( Log::Parser, "adding sender: ", addr, port );
		}
		break;
	default:
		FATAL(1)( Log::Fatal, "FATAL INTERNAL ERROR in CommandParser." );
		abortProcess();
	};
	LOG(3)( Log::Parser, "CP unreserve:", STYLE_Object(style),fdList.back() );
	api.releaseReservation( currentSession, style, fdList );
}

void CommandParser::closeCommand() {
	LOG(1)( Log::Parser, "CP close" );
	if ( (*currentSession)->getDestAddress().isMulticast() ) {
#if !defined(NS2)
		NetworkService::leaveMCastGroupIP4( dummyFD, (*currentSession)->getDestAddress() );
		close( dummyFD );
#endif
	}
	api.releaseSession( currentSession );
}

void CommandParser::sleepCommand() {
	uint32 seconds;
	*is >> seconds;
	LOG(2)( Log::Parser, "CP sleep:", seconds );
	api.sleep( TimeValue(seconds,0), endFlag );
	LOG(1)( Log::Parser, "CP woke up..." );
}

void CommandParser::waitPathCommand() {
	String srcAddrString;
	*is >> srcAddrString;
	if (srcAddrString == "any") {
		waitpathSender = FILTER_SPEC_Object::anyFilter;
	} else {
		NetAddress srcAddr( srcAddrString );
		uint16 srcPort = 0;
		*is >> srcPort;
		waitpathSender.setSrcAddress( srcAddr );
		waitpathSender.setLspId( srcPort );
	}
	LOG(2)( Log::Parser, "CP waitpath, waiting for", waitpathSender );
	gotPath = false;
	while ( !gotPath ) {
		if (NetworkService::waitForPacket( api.getFileDesc(), false )) {
			api.receiveAndProcess();
		}
	}
}

void CommandParser::ucpeCommand() {
	ucpe = new UCPE(0,0);
	*is >> *ucpe;
	LOG(2)( Log::Parser, "CP ucpe:", *ucpe );
}

bool CommandParser::readNextCommand() {
	*is >> token;
	return ( is->eof() || is->bad() || is->fail() ||
			token[0] == '#' ||
			token == "session" ||
			token == "sender" ||
#if defined(ONEPASS_RESERVATION)
			token == "senderresv" ||
#endif
			token == "reserve" ||
			token == "close" ||
			token == "waitpath" ||
			token == "sleep" ||
			token == "ucpe");
}

bool CommandParser::execNextCommand() {
#if !defined(NS2)
	if ( is->eof() ) {
		return false;
	}
#endif
	if ( token[0] == '#' ) {
		char line[256];
		is->getline( line, 255 );
		LOG(2)( Log::Parser, "CP comment:", line );
		readNextCommand();
	} else if ( token == "session" ) {
		sessionCommand();
		readNextCommand();
	} else if ( token == "sender" ) {
		senderCommand();
		readNextCommand();
	} else if ( token == "unsender" ) {
		unsenderCommand();
		readNextCommand();
#if defined(ONEPASS_RESERVATION)
	} else if ( token == "senderresv" ) {
		senderResvCommand();
#endif
	} else if ( token == "reserve" ) {
		reserveCommand();
	} else if ( token == "unreserve" ) {
		unreserveCommand();
	} else if ( token == "close" ) {
		closeCommand();
		readNextCommand();
	} else if ( token == "waitpath" ) {
		waitPathCommand();
		readNextCommand();
	} else if ( token == "sleep" ) {
		sleepCommand();
		readNextCommand();
	} else if ( token == "ucpe" ) {
		ucpeCommand();
		readNextCommand();
	} else {
#if !defined(NS2)
		ERROR(2)( Log::Error, "ERROR in command file, cannot understand command: ", token );
#endif
		return false;
	}
	return true;
}
