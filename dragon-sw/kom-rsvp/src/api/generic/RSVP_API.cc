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
#include "RSVP_API.h"
#include "RSVP_API_StateBlock.h"
#include "RSVP_API_Upcall.h"
#include "RSVP_FlowDescriptor.h"
#include "RSVP_Global.h"
#include "RSVP_IntServObjects.h"
#include "RSVP_Log.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_Message.h"
#include "RSVP_NetworkService.h"
#include "RSVP_PacketHeader.h"

LogicalInterfaceUDP* RSVP_API::apiLif = NULL;
int RSVP_API::apiRefCounter = 0;
	
inline ApiStateBlockList& RSVP_API::getStateList( const SESSION_Object& s ) {
	uint32 hash = s.getDestAddress().getHashValue(sessionHash);
	return stateList[hash];
}

RSVP_API::RSVP_API( const String& confFile, uint32 sessionHash )
	: sessionHash(sessionHash), apiPort(0) {
	uint16 apiPort = 0;
	NetAddress apiHost = LogicalInterface::loopbackAddress;
	ifstream ifs( confFile.chars() );
	String dummy;
	while ( ifs >> dummy ) {
		if ( dummy == "api" ) {
			ifs >> apiPort;
			ifs >> dummy;
			if ( !ifs.eof() && !ifs.bad() && !ifs.fail() && NetAddress::tryString( dummy ) ) {
				apiHost = dummy;
			}
	break;
		}
	}
	constructor( apiPort, apiHost );
}

void RSVP_API::constructor( uint16 apiPort, NetAddress apiHost ) {

	//@@@@ hack! >>Xi2007<<
	//if (apiLif) {
	//	FATAL(1)( Log::Fatal, "FATAL INTERNAL ERROR: only one instance of RSVP_API may be created" );
	//	abortProcess();
	//}

	//@@@@ hack! >>Xi2007<<
	if (!apiLif) // do global initializations
		RSVP_Global::init();

#if !defined(NS2)
	if ( apiPort == 0 ) apiPort = RSVP_Global::apiPort;
	if ( apiHost == NetAddress(0) ) apiHost = LogicalInterface::loopbackAddress;
#endif

	apiRefresh = RSVP_Global::defaultApiRefresh;

	stateList = new ApiStateBlockList[sessionHash];

	//@@@@ hack! >>Xi2007<<
	if (!apiLif) {
		apiLif = new LogicalInterfaceUDP( RSVP_Global::apiUniClientName, 0, RSVP_Global::apiMTU );
		PortList portList; portList.push_back( apiPort );
		apiLif->configureUDP( 0, apiHost, portList );
		apiLif->configureRefresh( 0 );
		apiLif->initWithPort();	
		LOG(2)( Log::API, "new API: ", *apiLif );
	}
	else {
		LOG(2)( Log::API, "use existing API: ", *apiLif );
	}

	//@@@@ hack! >>Xi2007<<
	apiRefCounter++;
}

RSVP_API::~RSVP_API() {
	uint32 i = 0;
	for ( ; i < sessionHash; i += 1 ) {
		ApiStateBlockList::ConstIterator iter = stateList[i].begin();
		ApiStateBlockList::ConstIterator nextIter;
		while ( iter != stateList[i].end() ) {
			nextIter = iter.next();
			releaseSession( iter );
			iter = nextIter;
		}
	}
	delete [] stateList;

	//@@@@ hack! >>Xi2007<<
	apiRefCounter--;
	if (apiRefCounter <= 0) {
		delete apiLif; apiLif = NULL;
	}
}

void RSVP_API::run( const bool& endFlag ) {
	while ( !endFlag ) {
		if (NetworkService::waitForPacket( apiLif->fd, false )) {
			receiveAndProcess();
		}
	}
}

void RSVP_API::sleep( TimeValue duration, const bool& endFlag ) {
	TimeValue startTime, endTime;
	while ( !endFlag && (duration > TimeValue(0,0)) ) {
		getCurrentSystemTime( startTime );
		if (NetworkService::waitForPacket( apiLif->fd, true, duration )) {
			receiveAndProcess();
		}
		getCurrentSystemTime( endTime );
		duration -= (endTime - startTime);
	}
}

void RSVP_API::receiveAndProcess(zUpcall upcall) {
	static INetworkBuffer buf( LogicalInterface::maxPayloadLength );
	buf.init();
	static Message msg;
	msg.init();
	static PacketHeader header;
	if ( apiLif->receiveBuffer( buf, header ) ) {
		if ( apiLif->parseBuffer( buf, header, msg ) ) {
			if ( msg.getStatus() == Message::Reject ) {
				LOG(4)( Log::Msg, "API ignoring message with status 'reject' from", header.getSrcAddress(), ":", msg );
			} else {
				process( msg , upcall);
			}
		}
	}
}

void RSVP_API::process( Message& msg , zUpcall upcall) {
#if defined(WITH_JAVA_API)
	preUpcall();
#endif
	GenericUpcallParameter upcallPara;
	upcallPara.generalInfo = NULL;

	const POLICY_DATA_Object* policyData = NULL;
	if ( !msg.getPolicyList().empty() ) {
		policyData = msg.getPolicyList().front();
	}
	SESSION_Object key(msg.getSESSION_Object());
	ApiStateBlockList::Iterator iter = getStateList(msg.getSESSION_Object()).find( &key );
	if ( iter != getStateList(msg.getSESSION_Object()).end() && (*iter)->upcall ) {
		switch( msg.getMsgType() ) {
			case Message::InitAPI:
				refreshSession( **iter );
	goto end;
			case Message::Path:
			case Message::PathResv:
				upcallPara.pathEvent = new UpcallParameterPATH_EVENT( iter,
					msg.getSENDER_TEMPLATE_Object(), msg.getSENDER_TSPEC_Object(),
					msg.getADSPEC_Object(), policyData );
				break;
			case Message::Resv:
				upcallPara.resvEvent = new UpcallParameterRESV_EVENT( iter,
					msg.getSTYLE_Object(), msg.getFlowDescriptorList().front(),
					policyData );
				break;
			case Message::PathTear:
				upcallPara.pathTear = new UpcallParameterPATH_TEAR( iter,
					msg.getSENDER_TEMPLATE_Object(), msg.getRSVP_HOP_Object(),
					msg.getPolicyList() );
				break;
			case Message::ResvTear:
				upcallPara.resvTear = new UpcallParameterRESV_TEAR( iter,
					msg.getFlowDescriptorList().front(),
					msg.getRSVP_HOP_Object(), msg.getPolicyList() );
				break;
			case Message::PathErr:
				upcallPara.pathError = new UpcallParameterPATH_ERROR( iter,
					msg.getSENDER_TEMPLATE_Object(), msg.getERROR_SPEC_Object(),
					msg.getPolicyList() );
				break;
			case Message::ResvErr:
				upcallPara.resvError = new UpcallParameterRESV_ERROR( iter,
					msg.getFlowDescriptorList().front(),
					msg.getERROR_SPEC_Object(), msg.getRSVP_HOP_Object(),
					msg.getPolicyList() );
				break;
			case Message::ResvConf:
				upcallPara.resvConfirm = new UpcallParameterRESV_CONFIRM( iter,
					msg.getSTYLE_Object(), msg.getERROR_SPEC_Object(),
					msg.getFlowDescriptorList().front(), msg.getPolicyList() );
				break;
			//@@@@ hacked
			case Message::AddLocalId:
			case Message::DeleteLocalId:
				break;
			default:
				FATAL(2)( Log::Fatal, "FATAL INTERNAL ERROR: daemon sent unknown message with type:", msg.getMsgType() );
				abortProcess();
		} // switch
		(*iter)->upcall( upcallPara, (*iter)->clientData ); //upcall to sendapi, etc.
		delete upcallPara.generalInfo;
	} // if
	if (upcall ) {
		struct _rsvp_upcall_parameter zUpcallParam;
		memset(&zUpcallParam, 0, sizeof(struct _rsvp_upcall_parameter));
		zUpcallParam.code = msg.getMsgType();
		zUpcallParam.destAddr.s_addr = key.getDestAddress().rawAddress();
		zUpcallParam.destPort = key.getTunnelId();
		zUpcallParam.srcAddr.s_addr = key.getExtendedTunnelId();
		if (msg.getMsgType()==Message::Path)
		{
			zUpcallParam.srcPort = msg.getSENDER_TEMPLATE_Object().getLspId();
			if (msg.hasSESSION_ATTRIBUTE_Object())
				zUpcallParam.name = msg.getSESSION_ATTRIBUTE_Object().getSessionName().chars();
			else
				zUpcallParam.name = NULL;
			if (msg.hasUPSTREAM_LABEL_Object())
				zUpcallParam.upstreamLabel = msg.getUPSTREAM_LABEL_Object().getLabel();
			else
				zUpcallParam.upstreamLabel = 0;
			zUpcallParam.bandwidth = floatMbitsToBytesInNetworkOrder(msg.getSENDER_TSPEC_Object().get_p());
			zUpcallParam.lspEncodingType = msg.getLABEL_REQUEST_Object().getLspEncodingType();
			zUpcallParam.switchingType = msg.getLABEL_REQUEST_Object().getSwitchingType();
			zUpcallParam.gPid = msg.getLABEL_REQUEST_Object().getGPid();
			SENDER_TSPEC_Object sts = SENDER_TSPEC_Object(msg.getSENDER_TSPEC_Object());
			zUpcallParam.sendTSpec =(void*)&sts;
			if (msg.getADSPEC_Object())
				zUpcallParam.adSpec = (void*)(msg.getADSPEC_Object());
			else
				zUpcallParam.adSpec = NULL;
			if ( iter != getStateList(msg.getSESSION_Object()).end())
				zUpcallParam.session = (void*)&iter;
			else
				zUpcallParam.session = NULL;

			if (msg.hasSUGGESTED_LABEL_Object() && (msg.getSUGGESTED_LABEL_Object().getLabel()>>16)==LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
				zUpcallParam.vlanTag = (msg.getSUGGESTED_LABEL_Object().getLabel() & 0xffff);
			else
				zUpcallParam.vlanTag = 0;

			SENDER_TEMPLATE_Object stm = SENDER_TEMPLATE_Object(msg.getSENDER_TEMPLATE_Object());
			zUpcallParam.senderTemplate = (void*)&stm;
		}
		else if (msg.getMsgType()==Message::PathErr || msg.getMsgType()==Message::ResvErr)
		{
		   zUpcallParam.errorSpecPara = new (struct _Error_Spec_Para); //mem leak
		   zUpcallParam.errorSpecPara->errFlags = msg.getERROR_SPEC_Object().getFlags();
		   zUpcallParam.errorSpecPara->errCode = msg.getERROR_SPEC_Object().getCode();
		   zUpcallParam.errorSpecPara->errValue = msg.getERROR_SPEC_Object().getValue();
		}

		if(msg.getDRAGON_UNI_Object())
		{
		   zUpcallParam.dragonUniPara = new (struct _Dragon_Uni_Para); //mem leak
		   zUpcallParam.dragonUniPara->srcLocalId = msg.getDRAGON_UNI_Object()->getSrcTNA().local_id;
		   zUpcallParam.dragonUniPara->destLocalId = msg.getDRAGON_UNI_Object()->getDestTNA().local_id;
		   zUpcallParam.dragonUniPara->vlanTag = msg.getDRAGON_UNI_Object()->getVlanTag().vtag;
		   memcpy(zUpcallParam.dragonUniPara->ingressChannel, msg.getDRAGON_UNI_Object()->getIngressCtrlChannel().name, 12);
		   memcpy(zUpcallParam.dragonUniPara->egressChannel, msg.getDRAGON_UNI_Object()->getEgressCtrlChannel().name, 12);
		   zUpcallParam.dragonUni = (void*)msg.getDRAGON_UNI_Object();
		}
		else
		{
		   zUpcallParam.dragonUniPara = NULL;
		   zUpcallParam.dragonUni = NULL;
		}

		if(msg.getDRAGON_EXT_INFO_Object())
		{
		   zUpcallParam.dragonExtInfoPara = new (struct _Dragon_ExtInfo_Para); //mem leak
		   memset(zUpcallParam.dragonExtInfoPara, 0, sizeof(struct _Dragon_ExtInfo_Para));
		   if (msg.getDRAGON_EXT_INFO_Object()->HasSubobj(DRAGON_EXT_SUBOBJ_SERVICE_CONF_ID))
		   {
			   zUpcallParam.dragonExtInfoPara->ucid = msg.getDRAGON_EXT_INFO_Object()->getServiceConfirmationID().ucid;
			   zUpcallParam.dragonExtInfoPara->seqnum = msg.getDRAGON_EXT_INFO_Object()->getServiceConfirmationID().seqnum;
		   }
		   if (msg.getDRAGON_EXT_INFO_Object()->HasSubobj(DRAGON_EXT_SUBOBJ_MON_NODE_LIST))
		   {
			   zUpcallParam.dragonExtInfoPara->num_mon_nodes = msg.getDRAGON_EXT_INFO_Object()->getMonNodeList().count;
			   zUpcallParam.dragonExtInfoPara->mon_nodes = new (struct in_addr)[msg.getDRAGON_EXT_INFO_Object()->getMonNodeList().count]; //mem reused by lsp->common->dragonExtInfoPara
			   memcpy(zUpcallParam.dragonExtInfoPara->mon_nodes, msg.getDRAGON_EXT_INFO_Object()->getMonNodeList().node_list, sizeof(struct in_addr)*msg.getDRAGON_EXT_INFO_Object()->getMonNodeList().count);
		   }
		   zUpcallParam.dragonExtInfo = (void*)msg.getDRAGON_EXT_INFO_Object();
		}
		else
		{
		   zUpcallParam.dragonExtInfoPara = NULL;
		   zUpcallParam.dragonExtInfo = NULL;
		}

		upcall(&zUpcallParam); //upcall to Zebra
	}

end:
#if defined(WITH_JAVA_API)
	postUpcall();
#endif
	return;
}

InterfaceHandle RSVP_API::getFileDesc() const {
	return apiLif->fd;
}

ApiStateBlockList::Iterator RSVP_API::findSession(SESSION_Object &session, UpcallProcedure upcall, void* clientData ) {

	ApiStateBlockList::Iterator iter = getStateList(session).find( &session );
	if ( iter != getStateList(session).end())
		return iter;
	else
		return NULL;
}

RSVP_API::SessionId RSVP_API::createSession( const NetAddress& dest, uint16 tunnelId, uint32 extTunnelID, UpcallProcedure upcall, void* clientData ) {
	SESSION_Object session( dest, tunnelId, extTunnelID);
	API_StateBlock *state = new API_StateBlock( session, upcall, clientData );
	RSVP_API::SessionId result = getStateList(session).lower_bound( state );
	if ( result == getStateList(session).end() || **result != *state ) {
		result = getStateList(session).insert( result, state );
	} else {
		delete state;
	}
	refreshSession( session, apiRefresh );
	return result;
}

//$$$$ DRAGON
void RSVP_API::addLocalId(uint16 type, uint16 value, uint16 tag)
{
	uint8 msgType = Message::AddLocalId;
	uint8 TTL = 1;
	SESSION_Object session(NetAddress(0), type, (value<<16)|tag);	
	Message msg( msgType, TTL, session);
	msg.setRSVP_HOP_Object( *apiLif );
	apiLif->sendMessage( msg, NetAddress(0), apiLif->getLocalAddress() );
}

//$$$$ DRAGON
void RSVP_API::deleteLocalId(uint16 type, uint16 value, uint16 tag)
{
	uint8 msgType = Message::DeleteLocalId;
	uint8 TTL = 1;
	SESSION_Object session(NetAddress(0), type, (value<<16)|tag);	
	Message msg( msgType, TTL, session);
	msg.setRSVP_HOP_Object( *apiLif );
	apiLif->sendMessage( msg, NetAddress(0), apiLif->getLocalAddress() );
}

//$$$$ DRAGON
void RSVP_API::refreshLocalId(uint16 type, uint16 value, uint16 tag)
{
	uint8 msgType = Message::RefreshLocalId;
	uint8 TTL = 1;
	SESSION_Object session(NetAddress(0), type, (value<<16)|tag);	
	Message msg( msgType, TTL, session);
	msg.setRSVP_HOP_Object( *apiLif );
	apiLif->sendMessage( msg, NetAddress(0), apiLif->getLocalAddress() );
}

//$$$$ DRAGON
void RSVP_API::monitoringQuery(uint32 destAddrIp, uint16 tunnelId, uint32 extTunnelId, char* gri)
{
	uint8 msgType = Message::RefreshLocalId;
	uint8 TTL = 1;
	const NetAddress destAddr(destAddrIp);
	SESSION_Object session(destAddr, (const uint16)tunnelId, (const uint32)extTunnelId);
	Message msgQuery( msgType, TTL, session);
	DRAGON_EXT_INFO_Object* dragonExtInfo = new DRAGON_EXT_INFO_Object;
	dragonExtInfo->SetMonQuery(gri);
	msgQuery.setDRAGON_EXT_INFO_Object(*dragonExtInfo);
	apiLif->sendMessage( msgQuery, NetAddress(0), apiLif->getLocalAddress() );	
	dragonExtInfo->destroy();
}

// the ip address in SENDER_TEMPLATE is set to 0, if no explicit one is given.
// this is adjusted by message processing in the daemon
void RSVP_API::createSender( SessionId iter, const NetAddress& addr, uint16 port, 
	const SENDER_TSPEC_Object& tspec, 
	const LABEL_REQUEST_Object&  labelReqObj, 
	EXPLICIT_ROUTE_Object* ero, 
	UNI_Object* uni,
	DRAGON_EXT_INFO_Object* dragonExtInfo,
	LABEL_SET_Object* labelSet, 
	SESSION_ATTRIBUTE_Object* ssAttrib, 
	UPSTREAM_LABEL_Object* upstreamLabel,
	uint8 TTL, 
	const ADSPEC_Object* adSpec,
	const POLICY_DATA_Object* policyData,
	bool reserve, uint16 senderRecvPort, uint16 recvSendPort ) {

	uint8 type = Message::Path;
#if defined(ONEPASS_RESERVATION)
	if ( reserve ) type = Message::PathResv;
#endif
	Message message( type, TTL, **iter );
	message.setSENDER_TEMPLATE_Object( SENDER_TEMPLATE_Object( addr, port ) );
	message.setSENDER_TSPEC_Object( tspec );
	message.setTIME_VALUES_Object( TimeValue(0,0) );
	message.setLABEL_REQUEST_Object(labelReqObj);
	if (ero) message.setEXPLICIT_ROUTE_Object(*ero);
	if (uni) {
		if (uni->getClassNumber() == RSVP_ObjectHeader::DRAGON_UNI)
			message.setUNI_Object(*(DRAGON_UNI_Object*)uni);
		else
			message.setUNI_Object(*(GENERALIZED_UNI_Object*)uni);
	}
	if (dragonExtInfo) {
		message.setDRAGON_EXT_INFO_Object(*dragonExtInfo);
	}
	if (labelSet) message.setLABEL_SET_Object(*labelSet);
	if (ssAttrib) message.setSESSION_ATTRIBUTE_Object(*ssAttrib);
	if (upstreamLabel) message.setUPSTREAM_LABEL_Object(*upstreamLabel);
	message.setRSVP_HOP_Object( *apiLif );
	if (adSpec) message.setADSPEC_Object( *adSpec );
	if (policyData) message.addPOLICY_DATA_Object( *policyData );
#if defined(ONEPASS_RESERVATION)
	if ( reserve && (senderRecvPort != 0 || recvSendPort != 0) ) {
		message.setDUPLEX_Object( DUPLEX_Object(senderRecvPort,recvSendPort) );
	}
#endif
	apiLif->sendMessage( message, (*iter)->getDestAddress(), addr );
}

void RSVP_API::createReservation( SessionId iter, bool confRequest,
	FilterStyle style, const FlowDescriptorList& fdList,
	const POLICY_DATA_Object* policyData, UNI_Object* uni, 
	DRAGON_EXT_INFO_Object* dragonExtInfo) {

	Message message( Message::Resv, 127, **iter );
	message.setRSVP_HOP_Object( *apiLif );
	message.setTIME_VALUES_Object( TimeValue(0,0) );
	if ( confRequest ) message.setRESV_CONFIRM_Object( NetAddress(0) );
	if ( policyData ) message.addPOLICY_DATA_Object( *policyData );
	message.setSTYLE_Object( style );
	FlowDescriptorList::ConstIterator flowdescIter = fdList.begin();
	for ( ; flowdescIter != fdList.end(); ++flowdescIter ) {
		if ( (*flowdescIter).getFlowspec() ) {
			message.addFLOWSPEC_Object( *(*flowdescIter).getFlowspec() );
		}
		message.addFILTER_SPEC_Objects( (*flowdescIter).filterSpecList );
	}
	if (uni) {
		if (uni->getClassNumber() == RSVP_ObjectHeader::DRAGON_UNI)
			message.setUNI_Object(*(DRAGON_UNI_Object*)uni);
		else
			message.setUNI_Object(*(GENERALIZED_UNI_Object*)uni);
	}
	if (dragonExtInfo) {
		message.setDRAGON_EXT_INFO_Object(*dragonExtInfo);
	}
	apiLif->sendMessage( message, NetAddress(0), apiLif->getLocalAddress() );
}

void RSVP_API::releaseSession( SessionId iter ) {
	SESSION_Object session = **iter;
	Message msg( Message::RemoveAPI, 127, session );
	msg.setRSVP_HOP_Object( *apiLif );
	delete *iter;
	getStateList(session).erase( iter );
	apiLif->sendMessage( msg, NetAddress(0), apiLif->getLocalAddress() );
}

void RSVP_API::releaseSession( SESSION_Object& session ) {
	uint32 i = 0;
	for ( ; i < sessionHash; i += 1 ) {
		ApiStateBlockList::ConstIterator iter = stateList[i].begin();
		ApiStateBlockList::ConstIterator nextIter;
		while ( iter != stateList[i].end() ) {
			nextIter = iter.next();
			if (**iter == session){
				releaseSession( iter );
				return;
			}
			iter = nextIter;
		}
	}
}

void RSVP_API::releaseSender( SessionId iter, const NetAddress& addr, uint16 port, uint8 TTL ) {

	Message message( Message::PathTear, TTL, **iter );
	message.setSENDER_TEMPLATE_Object( SENDER_TEMPLATE_Object( addr, port ) );
	message.setRSVP_HOP_Object( *apiLif );
	apiLif->sendMessage( message, (*iter)->getDestAddress(), addr );
}

void RSVP_API::releaseReservation( SessionId iter, FilterStyle style, const FlowDescriptorList& fdList ) {

	Message message( Message::ResvTear, 127, **iter );
	message.setRSVP_HOP_Object( *apiLif );
	message.setSTYLE_Object( style );
	FlowDescriptorList::ConstIterator flowdescIter = fdList.begin();
	for ( ; flowdescIter != fdList.end(); ++flowdescIter ) {
		if ( (*flowdescIter).getFlowspec() ) {
			message.addFLOWSPEC_Object( *(*flowdescIter).getFlowspec() );
		}
		message.addFILTER_SPEC_Objects( (*flowdescIter).filterSpecList );
	}
	apiLif->sendMessage( message, NetAddress(0), apiLif->getLocalAddress() );
}

void RSVP_API::releaseReservation( SESSION_Object& session, FilterStyle style, const FlowDescriptorList& fdList ) {
	uint32 i = 0;
	for ( ; i < sessionHash; i += 1 ) {
		ApiStateBlockList::ConstIterator iter = stateList[i].begin();
		ApiStateBlockList::ConstIterator nextIter;
		while ( iter != stateList[i].end() ) {
			nextIter = iter.next();
			if (**iter == session){
				Message message( Message::ResvTear, 127, **iter );
				message.setRSVP_HOP_Object( *apiLif );
				message.setSTYLE_Object( style );
				FlowDescriptorList::ConstIterator flowdescIter = fdList.begin();
				for ( ; flowdescIter != fdList.end(); ++flowdescIter ) {
					if ( (*flowdescIter).getFlowspec() ) {
						message.addFLOWSPEC_Object( *(*flowdescIter).getFlowspec() );
					}
					message.addFILTER_SPEC_Objects( (*flowdescIter).filterSpecList );
				}
				apiLif->sendMessage( message, NetAddress(0), apiLif->getLocalAddress() );
				return;
			}
			iter = nextIter;
		}
	}

}

void RSVP_API::refreshSession( const SESSION_Object& session, const TimeValue& refresh ) {

	Message msg( Message::InitAPI, 127, session );
	msg.setRSVP_HOP_Object( *apiLif );
	if ( refresh != TimeValue(0,0) ) msg.setTIME_VALUES_Object( refresh );
	apiLif->sendMessage( msg, NetAddress(0), apiLif->getLocalAddress() );
}

static void rsvpUpcall( const GenericUpcallParameter& upcallPara ) {
	cout << "***** UPCALL *****" << endl;
	switch( upcallPara.generalInfo->infoType ) {
		case UpcallParameter::PATH_EVENT:
			cout << "PATH_EVENT: " << *upcallPara.pathEvent << endl;
			break;
		case UpcallParameter::RESV_EVENT:
			cout << "RESV_EVENT: " << *upcallPara.resvEvent << endl;
			break;
		case UpcallParameter::PATH_TEAR:
			cout << "PATH_TEAR: " << *upcallPara.pathTear << endl;
			break;
		case UpcallParameter::RESV_TEAR:
			cout << "RESV_TEAR: " << *upcallPara.resvTear << endl;
			break;
		case UpcallParameter::PATH_ERROR:
			cout << "PATH_ERROR: " << *upcallPara.pathError << endl;
			break;
		case UpcallParameter::RESV_ERROR:
			cout << "RESV_ERROR: " << *upcallPara.resvError << endl;
			break;
		case UpcallParameter::RESV_CONFIRM:
			cout << "RESV_CONFIRM: " << *upcallPara.resvConfirm << endl;
			break;
		default:
			cerr << "upcall with unknown info type" << endl;
			break;
	}
}

void zInitRsvpPathRequest(void* thisApi, struct _sessionParameters* para, uint8 isSender)
{
	ADSPEC_Object *ao = NULL;
	SENDER_TSPEC_Object *stb = NULL;
	EXPLICIT_ROUTE_Object *ero = NULL;
	DRAGON_UNI_Object *uni = NULL;
	DRAGON_EXT_INFO_Object* dragonExtInfo = NULL;
	LABEL_SET_Object* labelSet = NULL;
	SESSION_ATTRIBUTE_Object* ssAttrib = NULL;
	UPSTREAM_LABEL_Object* upLabel = NULL;
	LABEL_REQUEST_Object *lr = NULL;
	RSVP_API *api = (RSVP_API *)thisApi;
	
       RSVP_API::SessionId session = 
       api->createSession( NetAddress(para->Session_Para.destAddr.s_addr), para->Session_Para.destPort, 
                                    para->Session_Para.srcAddr.s_addr, (UpcallProcedure)rsvpUpcall);

	//RSVP receiver stops here
	if (!isSender)
		return;
	
	if (para->ADSpec_Para){
	        ao = new ADSPEC_Object( para->ADSpec_Para->ADSpecHopCount, 
	         					      bytesInNetworkOrderToFloatMbits(para->ADSpec_Para->ADSpecBandwidth), 
	        					      para->ADSpec_Para->ADSpecMinPathLatency, 
	        					      para->ADSpec_Para->ADSpecMTU );
	        AdSpecCLParameters cl;
	        cl.override.setHopCount(para->ADSpec_Para->ADSpecCLHopCount);
	        cl.override.setMinPathLatency(para->ADSpec_Para->ADSpecCLMinPathLatency);
	        ao->addCL( cl );
	        AdSpecGSParameters gs( para->ADSpec_Para->ADSpecGSCtot, para->ADSpec_Para->ADSpecGSDtot,
	        					    para->ADSpec_Para->ADSpecGSCsum, para->ADSpec_Para->ADSpecGSDsum);
	        gs.override.setMinPathLatency(para->ADSpec_Para->ADSpecGSMinPathLatency);
	        ao->addGS( gs );
	}

	if (para->GenericTSpec_Para){
		 //This is a Generic GMPLS TSPEC
		 TSpec tb1(para->GenericTSpec_Para->R, para->GenericTSpec_Para->B, para->GenericTSpec_Para->P,
		 		   para->GenericTSpec_Para->m, para->GenericTSpec_Para->M);
	        stb = new SENDER_TSPEC_Object(tb1);
	}
	else if (para->SonetTSpec_Para){
		//This is a SONET/SDH GMPLS TSPEC
		SONET_TSpec sonet_tb1(para->SonetTSpec_Para->Sonet_ST, para->SonetTSpec_Para->Sonet_RCC, para->SonetTSpec_Para->Sonet_NCC,
							    para->SonetTSpec_Para->Sonet_NVC, para->SonetTSpec_Para->Sonet_MT, para->SonetTSpec_Para->Sonet_T,
							    para->SonetTSpec_Para->Sonet_P); 
	        stb = new SENDER_TSPEC_Object(sonet_tb1);
	}
	
	if (para->EROAbstractNode_Para && para->ERONodeNumber >= 2){
		ero = new EXPLICIT_ROUTE_Object();
		for (int i=0; i<para->ERONodeNumber; i++){
			switch (para->EROAbstractNode_Para[i].type){
				case AbstractNode::IPv4:
					ero->pushBack(AbstractNode(para->EROAbstractNode_Para[i].isLoose, 
								   NetAddress(para->EROAbstractNode_Para[i].data.ip4.addr.s_addr),
								   para->EROAbstractNode_Para[i].data.ip4.prefix));
					break;
				case AbstractNode::AS:
					ero->pushBack(AbstractNode(para->EROAbstractNode_Para[i].isLoose, 
								   NetAddress(para->EROAbstractNode_Para[i].data.asNum)));
					break;
				case AbstractNode::UNumIfID:
					ero->pushBack(AbstractNode(para->EROAbstractNode_Para[i].isLoose, 
								   NetAddress(para->EROAbstractNode_Para[i].data.uNumIfID.routerID.s_addr),
								   para->EROAbstractNode_Para[i].data.uNumIfID.interfaceID));
					break;
				default:
					break;
			}
		}
	}
	if (para->Dragon_Uni_Para) {
		uni = new DRAGON_UNI_Object(para->Session_Para.srcAddr, 
									para->Dragon_Uni_Para->srcLocalId, 
									para->Session_Para.destAddr, 
									para->Dragon_Uni_Para->destLocalId,
									para->Dragon_Uni_Para->vlanTag,
									para->Dragon_Uni_Para->ingressChannel,
									para->Dragon_Uni_Para->egressChannel);
	}
	if (para->Dragon_ExtInfo_Para) {
		dragonExtInfo = new DRAGON_EXT_INFO_Object;
		if ((para->Dragon_ExtInfo_Para->flags & EXT_INFO_FLAG_CONFIRMATION_ID) != 0 && para->Dragon_ExtInfo_Para->ucid != 0 && para->Dragon_ExtInfo_Para->seqnum != 0)
			dragonExtInfo->SetServiceConfirmationID(para->Dragon_ExtInfo_Para->ucid, para->Dragon_ExtInfo_Para->seqnum);
		if ((para->Dragon_ExtInfo_Para->flags & EXT_INFO_FLAG_SUBNET_EDGE_VLAN) != 0)
			dragonExtInfo->SetEdgeVlanMapping(para->Dragon_ExtInfo_Para->ingress_vtag, 0, 
                        (para->Dragon_ExtInfo_Para->ingress_vtag != 0 && para->Dragon_ExtInfo_Para->ingress_vtag != ANY_VTAG) ? para->Dragon_ExtInfo_Para->ingress_vtag : para->Dragon_ExtInfo_Para->egress_vtag, 0,
                        para->Dragon_ExtInfo_Para->egress_vtag, 0);
		if ((para->Dragon_ExtInfo_Para->flags & EXT_INFO_FLAG_SUBNET_DTL) != 0 && para->Dragon_ExtInfo_Para->dtl_hops != NULL)
			dragonExtInfo->SetDTL(para->Dragon_ExtInfo_Para->num_dlt_hops, para->Dragon_ExtInfo_Para->dtl_hops);
	}
	if (para->labelSet && para->labelSetSize > 0){
	        labelSet = new LABEL_SET_Object();
	        for (int i=0;i<para->labelSetSize;i++)
	        	labelSet->addSubChannel(para->labelSet[i]);
	}

	if (para->SessionAttribute_Para){
		ssAttrib = new SESSION_ATTRIBUTE_Object(String(para->SessionAttribute_Para->sessionName));
	}

	if (para->upstreamLabel){
		upLabel = new UPSTREAM_LABEL_Object(*para->upstreamLabel);
	}

	// Currently, only generalized GMPLS label and MPLS label are supported
	// No support of Waveband label!
	if (para->LabelRequest_Para.labelType == LABEL_Object::LABEL_GENERALIZED)
		lr = new LABEL_REQUEST_Object (para->LabelRequest_Para.data.gmpls.lspEncodingType, 
									  para->LabelRequest_Para.data.gmpls.switchingType,
							     		  para->LabelRequest_Para.data.gmpls.gPid);
	else
		lr = new LABEL_REQUEST_Object (para->LabelRequest_Para.data.mpls_l3pid);

       api->createSender( session, para->Session_Para.srcPort, *stb, *lr, ero, 
       			      uni, dragonExtInfo, labelSet, ssAttrib, upLabel, 50, ao, NULL );
	if (ao) ao->destroy();
	if (ero) ero->destroy();
	if (uni) uni->destroy();
	if (dragonExtInfo) dragonExtInfo->destroy();
	if (labelSet) labelSet->destroy();
	if (lr) delete lr;
	if (stb) delete stb;
	if (ssAttrib) delete ssAttrib;
	if (upLabel) delete upLabel;

	return;
}

void zInitRsvpResvRequest(void* api, struct _rsvp_upcall_parameter* upcallPara)
{
	const SENDER_TSPEC_Object* sentTSpec = (const SENDER_TSPEC_Object*)(upcallPara->sendTSpec);
	const ADSPEC_Object* adspec = (const ADSPEC_Object*)(upcallPara->adSpec);
	FLOWSPEC_Object* flowspec = NULL;
	ieee32float R = 0;

	if (sentTSpec->getService() == SENDER_TSPEC_Object::SonetSDH_Sender_Tspec)
		flowspec = new FLOWSPEC_Object((const SONET_TSpec&)(*sentTSpec));
	else{
		if ( adspec && adspec->supportsGS() ) {
			R = sentTSpec->calculateRate( adspec->getAdSpecGSParameters().getTotError(), 300000 );
		}
		if ( R != 0 ) {
			flowspec = new FLOWSPEC_Object( (const TSpec&)(*sentTSpec), RSpec( R, 0 ) );
		} else {
			flowspec = new FLOWSPEC_Object( (const TSpec&)(*sentTSpec) );
		}
	}
	FlowDescriptorList fdList;
	fdList.push_back( flowspec );
	fdList.back().filterSpecList.push_back( *((SENDER_TEMPLATE_Object*)(upcallPara->senderTemplate)) );
	if (!upcallPara->session){
		SESSION_Object session( NetAddress(upcallPara->destAddr.s_addr) , upcallPara->destPort, upcallPara->srcAddr.s_addr);
		ApiStateBlockList::Iterator iter  = ((RSVP_API*)api)->findSession(session);
		if (iter){
			upcallPara->session = (void*)&iter;
		}
		else{
			LOG(2)( Log::API, "API: Couldn't find session", session);
			return;
		}
	}
       //$$$$ Add DRAGON_UNI_Object if applicable
	((RSVP_API*)api)->createReservation( *((RSVP_API::SessionId*)(upcallPara->session)), false, FF, fdList, NULL, 
	(DRAGON_UNI_Object*)upcallPara->dragonUni, (DRAGON_EXT_INFO_Object*)upcallPara->dragonExtInfo);
}

void zTearRsvpPathRequest(void* api, struct _sessionParameters* para)
{
	
	SESSION_Object session(NetAddress(para->Session_Para.destAddr.s_addr), para->Session_Para.destPort, 
						    para->Session_Para.srcAddr.s_addr);	
	((RSVP_API*)api)->releaseSession(session);
	
	return;
}

void zTearRsvpResvRequest(void* api, struct _sessionParameters* para)
{
	/*
	  RFC 2205 3.1.6
	  Receipt of a ResvTear (reservation teardown) message deletes
         matching reservation state.  Matching reservation state must
         match the SESSION, STYLE, and FILTER_SPEC objects as well as
         the LIH in the RSVP_HOP object.  If there is no matching
         reservation state, a ResvTear message should be discarded.  A
         ResvTear message may tear down any subset of the filter specs
         in FF-style or SE-style reservation state.
       */
	SESSION_Object session(NetAddress(para->Session_Para.destAddr.s_addr), para->Session_Para.destPort, 
							para->Session_Para.srcAddr.s_addr); 
	FlowDescriptorList fdList;
	fdList.push_back( FlowDescriptor::FlowDescriptor() );
	fdList.back().filterSpecList.push_back(FILTER_SPEC_Object::FILTER_SPEC_Object( NetAddress(para->Session_Para.srcAddr.s_addr),
									para->Session_Para.srcPort ) );
	
	((RSVP_API*)api)->releaseReservation(session, FF, fdList);
	return;
}

void zInitRsvpReceiverRequest(void* thisApi, struct _sessionParameters* para)
{
	RSVP_API *api = (RSVP_API *)thisApi;
	
       api->createSession( NetAddress(para->Session_Para.destAddr.s_addr), para->Session_Para.destPort, 
                                    para->Session_Para.srcAddr.s_addr, (UpcallProcedure)rsvpUpcall);
	return;
}

int zGetApiFileDesc(void *api)
{
	return ((RSVP_API *)api)->getFileDesc();
}

void zApiReceiveAndProcess(void *api, zUpcall upcall)
{
	((RSVP_API *)api)->receiveAndProcess(upcall);
}

void* zInitRsvpApiInstance()
{
       Log::init( "all", "ref,packet,select" );
	RSVP_API *api = new RSVP_API();
	return api;
}

void zAddLocalId(void* api, uint16 type, uint16 value, uint16 tag)
{
    ((RSVP_API *)api)->addLocalId(type, value, tag);
}
void zDeleteLocalId(void* api, uint16 type, uint16 value, uint16 tag)
{
    ((RSVP_API *)api)->deleteLocalId(type, value, tag);
}
void zRefreshLocalId(void* api, uint16 type, uint16 value, uint16 tag)
{
    ((RSVP_API *)api)->refreshLocalId(type, value, tag);
}
void zMonitoringQuery(void* api, uint32 destAddrIp, uint16 tunnelId, uint32 extTunnelId, char* gri)
{
	((RSVP_API *)api)->monitoringQuery(destAddrIp, tunnelId, extTunnelId, gri); //destIP, destPort, sourceIP, lspName
}
