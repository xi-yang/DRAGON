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
#include "RSVP_API_Server.h"
#include "RSVP.h"
#include "RSVP_Global.h"
#include "RSVP_Log.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_Message.h"
#include "RSVP_MessageProcessor.h"
#include "RSVP_MPLS.h"
#include "RSVP_RoutingService.h"
#include "RSVP_PHopSB.h"
#include "RSVP_OIatPSB.h"
#include "RSVP_OutISB.h"
#include "RSVP_RSB.h"
#include "RSVP_Session.h"
#include "RSVP_FilterSpecList.h"
#include "RSVP_TrafficControl.h"
#include "NARB_APIClient.h"
#include "SwitchCtrl_Global.h"
//#include "RSVP_UNI.h"

// #define BETWEEN_APIS 1

Session::Session( const SESSION_Object &session) : SESSION_Object(session),
	style(None), rsbCount(0) {
	LOG(2)( Log::Session, "new Session:", *this );
	outLif = NULL;
	inLif = NULL;
	gw = LogicalInterface::noGatewayAddress;
	biDir = false;
}

Session::~Session() {
	LOG(2)( Log::Session, "delete Session:", *this );
	RSVP_Global::rsvp->removeSession( iterFromRSVP );
	RSVP_Global::rsvp->getMPLS().deleteExplicitRouteBySession((uint32)this); //@@@@ use address as unique ID.
}

void Session::deleteAll() {
	LOG(2)( Log::Session, "cleanup session: ", *this );
	bool sessionDeleted = (RelationshipSession_PSB::followRelationship().size() == 0);
	while ( !sessionDeleted ) {
		sessionDeleted = (RelationshipSession_PSB::followRelationship().size() == 1);
		PSB_List::Iterator iterPSB = RelationshipSession_PSB::followRelationship().begin();
		delete *iterPSB;
	}
}

#if defined(USE_SCOPE_OBJECT)
void Session::matchPSBsAndSCOPEandOutInterface( const AddressList& addressList, const LogicalInterface& OI, PSB_List& result, OutISB*& resultOI ) {
	resultOI = NULL;
	AddressList::ConstIterator addressIter = addressList.begin();
	PSB_List::Iterator psbIter = RelationshipSession_PSB::followRelationship().begin();
	while ( addressIter != addressList.end() && psbIter != RelationshipSession_PSB::followRelationship().end() ) {
		if ( *addressIter < (*psbIter)->getSrcAddress() ) {
			++addressIter;
		} else if ( (*psbIter)->getSrcAddress() < *addressIter || !(*psbIter)->matchOI( OI ) ) {
			++psbIter;
#if defined(WITH_API)
		} else if ( (*psbIter)->isLocalOnly() && &OI != RSVP::getApiLif() ) {
			++psbIter;
#endif
		} else {
			LOG(2)( Log::Process, "matched", **psbIter );
			result.push_back( *psbIter );
			if ( !resultOI ) resultOI = (*psbIter)->getOutISB( OI.getLIH() );
			++psbIter;
		}
	}
}
#endif

void Session::matchPSBsAndFiltersAndOutInterface( const FilterSpecList& filterList, const LogicalInterface& OI, PSB_List& result, OutISB*& resultOI ) {
	resultOI = NULL;
	FilterSpecList::ConstIterator filterIter = filterList.begin();
	PSB_List::Iterator psbIter = RelationshipSession_PSB::followRelationship().begin();
	while ( filterIter != filterList.end() && psbIter != RelationshipSession_PSB::followRelationship().end() ) {
		if ( style == SE && !resultOI ) {
			resultOI = (*psbIter)->getOutISB( OI.getLIH() );
		}
		if ( *filterIter < **psbIter ) {
			++filterIter;
		} else if ( **psbIter < *filterIter  || !(*psbIter)->matchOI( OI )) {
			++psbIter;
#if defined(WITH_API)
		} else if ( (*psbIter)->isLocalOnly() && &OI != RSVP::getApiLif() ) {
			++psbIter;
#endif
		} else {
			LOG(2)( Log::Process, "matched", **psbIter );
			result.push_back( *psbIter );
			if ( (*psbIter)->getOIatPSB(OI.getLIH())->hasOutLabelRequested()
				&& (*filterIter).hasLabel() && OI.hasEnabledMPLS() 
				&& (*filterIter).getLABEL_Object().getLabelCType() ==  (*psbIter)->getOIatPSB(OI.getLIH())->getRequestedOutLabelType()) {
				(*psbIter)->setOutLabel( (*filterIter).getLABEL_Object().getLabel() );
			} else {
				(*psbIter)->setOutLabel( 0 );
			}
			if ( !resultOI ) {
				resultOI = (*psbIter)->getOutISB( OI.getLIH() );
			}
			++filterIter; ++psbIter;
		}
	}
	if ( style == SE ) {
		while ( !resultOI && psbIter != RelationshipSession_PSB::followRelationship().end() ) {
			resultOI = (*psbIter)->getOutISB( OI.getLIH() );
			++psbIter;
		}
	}
	if ( filterList.empty() && style == WF ) {
		for (; psbIter != RelationshipSession_PSB::followRelationship().end(); ++psbIter ) {
			if ( (*psbIter)->matchOI( OI ) ) {
				LOG(2)( Log::Process, "matched", **psbIter );
				result.push_back( (*psbIter) );
				if ( !resultOI ) {
					resultOI = (*psbIter)->getOutISB( OI.getLIH() );
				}
			}
		}
	}
}

PHopSB* Session::findOrCreatePHopSB( Hop& hop, uint32 LIH ) {

	PHopSBKey key( hop, LIH );
	PHOP_List::Iterator phopIter = RelationshipSession_PHopSB::followRelationship().lower_bound( &key );
	if ( (phopIter != RelationshipSession_PHopSB::followRelationship().end() && (**phopIter == key))
	) {
//		LOG(2)( Log::Process, "found", **phopIter );
		return *phopIter;
	} else {
		PHopSB* phopState = new PHopSB( hop, LIH );
		RelationshipSession_PHopSB::setRelationshipFull( this, phopState, phopIter );
		return phopState;
	}
}

OutISB* Session::findOutISB( const LogicalInterface& lif, const PSB& sender ) {
	PSB_List::Iterator psbIter = RelationshipSession_PSB::followRelationship().begin();
	for (; psbIter != RelationshipSession_PSB::followRelationship().end(); ++psbIter ) {
		if ( **psbIter != sender && (*psbIter)->matchOI( lif ) ) {
				return (*psbIter)->getOutISB( lif.getLIH() );
		}
	}
	return NULL;
}

bool Session::processERO(const Message& msg, Hop& hop, EXPLICIT_ROUTE_Object* explicitRoute, bool fromLocalAPI, RSVP_HOP_Object& dataInRsvpHop, RSVP_HOP_Object& dataOutRsvpHop, VLSRRoute& vLSRoute)
{
	NetAddress phopLoopBackAddr;
	const LogicalInterface* outLif = NULL;
	uint32 ifId;
	RSVP_HOP_TLV_SUB_Object tlv;
	uint32 inUnumIfID = 0;
	uint32 outUnumIfID = 0;
	NetAddress inRtId = NetAddress(0);
	NetAddress outRtId = NetAddress(0);
       NetAddress gw;

	if (!fromLocalAPI){
		if (!RSVP_Global::rsvp->getRoutingService().findDataByInterface(hop.getLogicalInterface(), inRtId, inUnumIfID))
			return false;
		
                //E2E tagged VLAN processing (inbound) @@@@ hacked
		if (explicitRoute->getAbstractNodeList().size() > 1 
		&& explicitRoute->getAbstractNodeList().front().getType() == AbstractNode::UNumIfID 
		&& explicitRoute->getAbstractNodeList().front().getInterfaceID() >> 16 == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
		{
			inUnumIfID = explicitRoute->getAbstractNodeList().front().getInterfaceID();
                        RSVP_HOP_TLV_SUB_Object t(inRtId);
			tlv = t;
		}
		else {
			if (!inUnumIfID) { //numbered interface
				RSVP_HOP_TLV_SUB_Object t(inRtId);
				tlv = t;
			}
			else { //un-numbered interface
				tlv = msg.getRSVP_HOP_Object().getTLV();
                        }
		}
		dataInRsvpHop = RSVP_HOP_Object(hop.getLogicalInterface().getAddress(), msg.getRSVP_HOP_Object().getLIH(), tlv);
	}
       else  //ingress localID processing @@@@ hacked
       {
           if (explicitRoute->getAbstractNodeList().front().getType()==AbstractNode::UNumIfID)
           {
               if (explicitRoute->getAbstractNodeList().front().getAddress() == RSVP_Global::rsvp->getRoutingService().getLoopbackAddress())
                    inUnumIfID = explicitRoute->getAbstractNodeList().front().getInterfaceID();
           }
       }

	while (explicitRoute->getAbstractNodeList().size() )
	{
		ifId = explicitRoute->getAbstractNodeList().front().getType()==AbstractNode::IPv4?0:explicitRoute->getAbstractNodeList().front().getInterfaceID();
		if ((ifId >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
			ifId = 0;
		NetAddress hopAddr = explicitRoute->getAbstractNodeList().front().getAddress();
		if (hopAddr != RSVP_Global::rsvp->getRoutingService().getLoopbackAddress()) {
			outLif = RSVP_Global::rsvp->getRoutingService().findInterfaceByData(hopAddr, ifId); 
			if (!outLif)
			{
				outLif = RSVP_Global::rsvp->findInterfaceByAddress(hopAddr);
				if (!outLif)  break;
			}
		} else if (explicitRoute->getAbstractNodeList().size() == 1 && explicitRoute->getAbstractNodeList().front().getType()==AbstractNode::UNumIfID) {
	             //egress localID processing @@@@ hacked
                    outUnumIfID = explicitRoute->getAbstractNodeList().front().getInterfaceID();
              }
		explicitRoute->popFront();
		// E2E tagged VLAN processing (outbound) @@@@ hacked
		if (explicitRoute->getAbstractNodeList().size() > 1 
		&& explicitRoute->getAbstractNodeList().front().getType() == AbstractNode::UNumIfID 
		&& explicitRoute->getAbstractNodeList().front().getInterfaceID() >> 16 == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
			outUnumIfID = explicitRoute->getAbstractNodeList().front().getInterfaceID();
	}

	if (!explicitRoute->getAbstractNodeList().empty()){
		//process loose hop
		if ( explicitRoute->getAbstractNodeList().front().isLoose() ){
		 	if (explicitRoute->getAbstractNodeList().front().getType() != AbstractNode::IPv4 ||
		 	     explicitRoute->getAbstractNodeList().front().getType() != AbstractNode::UNumIfID)
			{
				LOG(2)( Log::MPLS, "MPLS: loose hop ero not supported for abstract node type ", explicitRoute->getAbstractNodeList().front().getType() );
				return false;
			}
			else 
			{
				LOG(2)( Log::MPLS, "MPLS: resolving loose hop by local routing module: ", explicitRoute->getAbstractNodeList().front() );
                            EXPLICIT_ROUTE_Object* ero = NULL;
				/* @@@@ Use of NARB in loose hop expansion yet to be determinted
                            uint32 vtag = (explicitRoute->getAbstractNodeList().front().getInterfaceID() >> 16 == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL) ?
                                   explicitRoute->getAbstractNodeList().front().getInterfaceID() & 0xffff : 0;
                            if (NARB_APIClient::instance().operational())
                                ero = NARB_APIClient::instance().getExplicitRoute(RSVP_Global::rsvp->getRoutingService().getLoopbackAddress().rawAddress(), 
                                    getDestAddress().rawAddress(), msg.getLABEL_REQUEST_Object().getSwitchingType(), msg.getLABEL_REQUEST_Object().getLspEncodingType(), 
                                    msg.getSENDER_TSPEC_Object().get_r(), vtag);
                            */
                            if (!ero)
    				    ero  = RSVP_Global::rsvp->getRoutingService().getExplicitRouteByOSPF(
											hop.getLogicalInterface().getAddress(),
											explicitRoute->getAbstractNodeList().front().getAddress(), 
											msg.getSENDER_TSPEC_Object(), msg.getLABEL_REQUEST_Object());
				if (ero && (!ero->getAbstractNodeList().front().isLoose())){
					explicitRoute->popFront();
					while (!ero->getAbstractNodeList().empty()){
						//outLif = RSVP_Global::rsvp->getRoutingService().findOutLifByOSPF(
						//	     ero->getAbstractNodeList().back().getAddress(),ifId, gw);
						//if (!outLif)
							explicitRoute->pushFront(ero->getAbstractNodeList().back());
						//ero->popBack();
					}
				}
				else{
					LOG(2)( Log::MPLS, "MPLS: unable to resolve loose hop:", explicitRoute->getAbstractNodeList().front() );
					return false;
				}
			}
		}
		
		ifId = explicitRoute->getAbstractNodeList().front().getType()==AbstractNode::IPv4?0:explicitRoute->getAbstractNodeList().front().getInterfaceID();
		if ((ifId >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
			ifId = 0;
		outLif = RSVP_Global::rsvp->getRoutingService().findOutLifByOSPF(
			     explicitRoute->getAbstractNodeList().front().getAddress(), ifId, gw);
		if (!outLif)
			return false;
		
		if (!RSVP_Global::rsvp->getRoutingService().findDataByInterface(*outLif, outRtId, outUnumIfID))
			return false;

		phopLoopBackAddr = RSVP_Global::rsvp->getRoutingService().getLoopbackAddress();
		if (!outUnumIfID || ifId == 0) { //numbered interface
			RSVP_HOP_TLV_SUB_Object t(outRtId);
			tlv = t;
		}
		else { //un-numbered interface
			RSVP_HOP_TLV_SUB_Object t(outRtId, outUnumIfID);
			tlv  = t;
		}
		if (fromLocalAPI)
			dataOutRsvpHop = RSVP_HOP_Object(phopLoopBackAddr, outLif->getLIH(), tlv );		
		else
			dataOutRsvpHop = RSVP_HOP_Object(outLif->getAddress(), outLif->getLIH(), tlv );		

		if (dataOutRsvpHop.getAddress() ==NetAddress(0) || ((!fromLocalAPI) && dataOutRsvpHop.getAddress()==NetAddress(0)))
			return false;		
	}
	//check if this represents a VLSR route
	if ((fromLocalAPI && inUnumIfID!= 0 && outRtId != NetAddress(0)) ||
          (!fromLocalAPI && inRtId != NetAddress(0) && outUnumIfID != 0) ||
	   (((!fromLocalAPI) && inRtId != NetAddress(0) && outRtId != NetAddress(0)) || (inRtId == outRtId && outUnumIfID != 0)) )
	{	
		SwitchCtrlSessionList::Iterator sessionIter;
		bool foundSession;
		SwitchCtrl_Session* ssNew;
		VLSR_Route vlsr;
		memset(&vlsr, 0, sizeof(VLSR_Route));
		RSVP_Global::rsvp->getRoutingService().getVLSRRoutebyOSPF(inRtId, outRtId, inUnumIfID, outUnumIfID, vlsr);
		vlsr.bandwidth = msg.getSENDER_TSPEC_Object().get_r(); //bandwidth in Mbps (* 1000000/8 => Bps)
		if (vlsr.inPort && vlsr.outPort && vlsr.switchID != NetAddress(0))
		{
			//prepare SwitchCtrl session connection
			sessionIter = RSVP_Global::switchController->getSessionList().begin();
			foundSession = false;
			for (; sessionIter != RSVP_Global::switchController->getSessionList().end(); ++sessionIter ) {
				if ((*sessionIter)->getSwitchInetAddr()==vlsr.switchID && (*sessionIter)->isValidSession()){
					foundSession = true;
					break;
				}
			}
			if (!foundSession){
				LOG(2)( Log::MPLS, "VLSR: SwitchCtrl Session not found. Now creating new session for ", vlsr.switchID);

                            ssNew = RSVP_Global::switchController->createSession(vlsr.switchID);
                            if (!ssNew) {
                                    LOG(1)( Log::MPLS, "Failed to create VLSR SwitchCtrl Session: Unkown switch Vendor/Model");
        				return false;
                            }

				if (!ssNew->connectSwitch()){
					LOG(2)( Log::MPLS, "VLSR: Cannot connect to Ethernet switch : ", vlsr.switchID);
					delete(ssNew);
					return false;
				}
				else{
					RSVP_Global::switchController->addSession(ssNew);
				}
			}
			else if (!(*sessionIter)->readVLANFromSwitch()) { //Read/Synchronize to Ethernet switch
			       //syncWithSwitch ... !
				LOG(2)( Log::MPLS, "VLSR: Cannot read from Ethernet switch : ", vlsr.switchID);
				return false;
			}
			vLSRoute.push_back(vlsr);                    
		}
	}
       // local-id processing for ingress => allocation of inLabel for local ingress port(s) @@@@ hacked
	if (fromLocalAPI && inUnumIfID!= 0 && outRtId != NetAddress(0))
       {   
            LogicalInterface *lif = (LogicalInterface*)&hop.getLogicalInterface();
            lif->setMPLS(true);
       }

	return true;
}

bool Session::shouldReroute( const EXPLICIT_ROUTE_Object* ero ) {
	LogicalInterface* outLif;
	uint32 ifId;

	//if destination is reached, there is no need for nexthop route
	NetAddress loopback =  RSVP_Global::rsvp->getRoutingService().getLoopbackAddress();
	if (getDestAddress() == loopback)
		return false;

	//skip all local hops
	AbstractNodeList::ConstIterator iter = ero->getAbstractNodeList().begin();
	for (; iter != ero->getAbstractNodeList().end(); ++iter){
		switch ((*iter).getType()) {
		case AbstractNode::IPv4:
			ifId = 0;
			break;
		case AbstractNode::UNumIfID:
			ifId = (*iter).getInterfaceID();
			break;
		default:
			return false;
		}
		if ((*iter).getAddress() != loopback) {
			outLif = (LogicalInterface*)RSVP_Global::rsvp->getRoutingService().findInterfaceByData((*iter).getAddress(), ifId); 
			if (!outLif)
			{
				outLif = (LogicalInterface*)RSVP_Global::rsvp->findInterfaceByAddress((*iter).getAddress());
				if (!outLif)  break;
			}
		}
	}

	//no nexthop found and we do need it
	if (iter == ero->getAbstractNodeList().end())
		return true;
	//expanding loose hop
	if ((*iter).isLoose())
		return true;
	//rerouting for a hop OSPFd cannot resolve
	outLif = (LogicalInterface*)RSVP_Global::rsvp->getRoutingService().findOutLifByOSPF(
		     (*iter).getAddress(), ifId, gw);
	if (!outLif) return true;

	//no nexthop routing
	return false;
}


void Session::processPATH( const Message& msg, Hop& hop, uint8 TTL ) {

#if defined(ONEPASS_RESERVATION)
	SENDER_TEMPLATE_Object& senderTemplate =
		const_cast<SENDER_TEMPLATE_Object&>(msg.getSENDER_TEMPLATE_Object());
#else
	SENDER_TEMPLATE_Object senderTemplate = msg.getSENDER_TEMPLATE_Object();
#endif
	uint32 phopLIH = msg.getRSVP_HOP_Object().getLIH();
	bool Path_Refresh_Needed = false;

	NetAddress loopback = RSVP_Global::rsvp->getRoutingService().getLoopbackAddress();

#if defined(WITH_API)
/* OLD @@@@
	bool fromLocalAPI = (&hop.getLogicalInterface() == RSVP_Global::rsvp->getApiLif()
		&& (msg.getRSVP_HOP_Object().getAddress() == LogicalInterface::loopbackAddress
		|| RSVP_Global::rsvp->findInterfaceByAddress(msg.getRSVP_HOP_Object().getAddress())) );
	bool ingressVLSR = fromLocalAPI? false : msg.getRSVP_HOP_Object().getAddress() == loopback;
*/
	bool fromLocalAPI = (&hop.getLogicalInterface() == RSVP_Global::rsvp->getApiLif()
		&& (msg.getRSVP_HOP_Object().getAddress() == LogicalInterface::loopbackAddress
		|| msg.getRSVP_HOP_Object().getAddress() == loopback
		|| RSVP_Global::rsvp->findInterfaceByAddress(msg.getRSVP_HOP_Object().getAddress())));

	bool fromUniClient = (&hop.getLogicalInterface() == RSVP_Global::rsvp->getApiLif()
						&&((Message*)&msg)->getDRAGON_UNI_Object() != NULL);
#endif

	LogicalInterfaceSet RtOutL;
	const LogicalInterface* RtInIf = &hop.getLogicalInterface();
	const LogicalInterface* defaultOutLif = NULL;
	NetAddress gateway = LogicalInterface::noGatewayAddress;
	const POLICY_DATA_Object* policyData = NULL;
	NetAddress destAddress = getDestAddress();
	// TODO: currently only strict IPv4 addressing is supported
	RSVP_HOP_Object dataInRsvpHop;
	RSVP_HOP_Object dataOutRsvpHop;
	VLSRRoute vLSRoute;

	EXPLICIT_ROUTE_Object* explicitRoute = NULL;

	if (fromUniClient) {
                DRAGON_UNI_Object* uni = ((Message*)&msg)->getDRAGON_UNI_Object();
                defaultOutLif = RSVP_Global::rsvp->findInterfaceByName(String((const char*)uni->getCtrlChannelName().name));
                RtOutL.insert_unique( defaultOutLif );
                RSVP_Global::rsvp->getRoutingService().getPeerIPAddr(defaultOutLif->getLocalAddress(), gateway);
                RSVP_HOP_TLV_SUB_Object tlv (NetAddress(0));
                dataOutRsvpHop = RSVP_HOP_Object(NetAddress(uni->getSrcTNA().addr.s_addr), defaultOutLif->getLIH(), tlv);
                senderTemplate.setSrcAddress(NetAddress(uni->getSrcTNA().addr.s_addr));
	}
	else {

		if (!RSVP_Global::rsvp->getRoutingService().getOspfSocket()){
			RSVP_Global::messageProcessor->sendPathErrMessage( ERROR_SPEC_Object::RoutingProblem, ERROR_SPEC_Object::NoRouteAvailToDest);
			return;
		}

		if ( destAddress.isMulticast() ) {
			LOG(2)( Log::MPLS, "MPLS: explicit routing not allowed for multicast sessions:", destAddress );
			explicitRoute = NULL;
		} 
		else{
			explicitRoute = ((!fromLocalAPI) && (!hop.getLogicalInterface().hasEnabledMPLS())) ? NULL
						   : const_cast<EXPLICIT_ROUTE_Object*>(msg.getEXPLICIT_ROUTE_Object());

			//preprocess ERO
			if (explicitRoute && shouldReroute(explicitRoute)) {
				explicitRoute->destroy();
				explicitRoute = NULL;
			}
			if (!explicitRoute) {
				LOG(2)( Log::MPLS, "MPLS: requesting ERO from local routing module...", *static_cast<SESSION_Object*>(this));

	                     //explicit routing using local configuration
	                    	explicitRoute = RSVP_Global::rsvp->getMPLS().getExplicitRoute(destAddress);

	                     //explicit routing using NARB
	                     if (!explicitRoute && NARB_APIClient::instance().operational()) {
	                            explicitRoute = NARB_APIClient::instance().getExplicitRoute(loopback.rawAddress(), 
	                                    getDestAddress().rawAddress(), msg.getLABEL_REQUEST_Object().getSwitchingType(), msg.getLABEL_REQUEST_Object().getLspEncodingType(), 
	                                    msg.getSENDER_TSPEC_Object().get_r(), 0);
		                     //@@@@ set vtag == 0 for now. We need a mechanism to pass vtag request in some RESV object (LABEL_REQUEST_Object? SENDER_TSPEC? Or a new Ether_TSPEC!).

		                     //explicit routing using OSPFd
		                     if (!explicitRoute)
					       explicitRoute = RSVP_Global::rsvp->getRoutingService().getExplicitRouteByOSPF(
								 hop.getLogicalInterface().getAddress(),
								 destAddress, msg.getSENDER_TSPEC_Object(), msg.getLABEL_REQUEST_Object());

					//store ERO into erList //@@@@ to be changed ...
					if (explicitRoute) {
						SimpleList<NetAddress> simple_ero;
						AbstractNodeList::ConstIterator iter = explicitRoute->getAbstractNodeList().begin();
						LogicalInterface * aLif;
						for (; iter != explicitRoute->getAbstractNodeList().end(); ++iter){
							if ((*iter).getAddress() != loopback) {
								aLif = (LogicalInterface*)RSVP_Global::rsvp->getRoutingService().findInterfaceByData((*iter).getAddress(), 0); 
								if (!aLif)
								{
									simple_ero.push_back((*iter).getAddress());
								}
							}
						}
						//@@@@use address as unique ID. Later on when the session quits, this ERO is removed from erList
						RSVP_Global::rsvp->getMPLS().addExplicitRoute(destAddress, simple_ero, (uint32)this);
					}
	                    	}
			}
			else{
				// further, all error conditions should return an error and ignore the message
				assert( !explicitRoute->getAbstractNodeList().empty() );
				if (explicitRoute->getAbstractNodeList().front().getType() != AbstractNode::IPv4 &&
	 			     explicitRoute->getAbstractNodeList().front().getType() != AbstractNode::UNumIfID)
				{
					LOG(2)( Log::MPLS, "MPLS: abstract node type not supported yet:", explicitRoute->getAbstractNodeList().front().getType() );
					explicitRoute = NULL;
				}
			}
		}

		if (explicitRoute && (!processERO(msg, hop, explicitRoute, fromLocalAPI, dataInRsvpHop, dataOutRsvpHop, vLSRoute)))
		{
			LOG(2)(Log::MPLS, "MPLS: Internal error in the ERO :", explicitRoute->getAbstractNodeList().front().getAddress());
			explicitRoute = NULL;
		}

		explicitRoute = RSVP_Global::rsvp->getMPLS().updateExplicitRoute( destAddress, explicitRoute );
		if ( explicitRoute && !explicitRoute->getAbstractNodeList().empty()) {
			destAddress = explicitRoute->getAbstractNodeList().front().getAddress();
		}
		else{
			ERROR(2)( Log::Error, "Can't determine data interfaces!", *static_cast<SESSION_Object*>(this));
			RSVP_Global::messageProcessor->sendPathErrMessage( ERROR_SPEC_Object::RoutingProblem, ERROR_SPEC_Object::BadExplicitRoute );
			return;
		}

#if defined(ONEPASS_RESERVATION)
		if ( msg.getMsgType() == Message::PathResv && RSVP_Global::messageProcessor->getOnepassPSB() ) {
			RtInIf = NULL;
			RtOutL.insert_unique( &hop.getLogicalInterface() );
		goto search_psb;
		}
#endif

#if defined(WITH_API)
		if ( fromLocalAPI ) {
			// message is from local API -> set sender address if not set
			if (explicitRoute->getAbstractNodeList().front().getType() == AbstractNode::IPv4
			|| (explicitRoute->getAbstractNodeList().front().getType() == AbstractNode::UNumIfID 
			&& (explicitRoute->getAbstractNodeList().front().getInterfaceID()>>16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL))
				defaultOutLif = RSVP_Global::rsvp->getRoutingService().findOutLifByOSPF(destAddress, 0, gateway);
			else if (explicitRoute->getAbstractNodeList().front().getType() == AbstractNode::UNumIfID)
			{
				uint32 uNumIfID = explicitRoute->getAbstractNodeList().front().getInterfaceID();
				defaultOutLif = RSVP_Global::rsvp->getRoutingService().findOutLifByOSPF(destAddress, uNumIfID, gateway);
			}
			else
				defaultOutLif = RSVP_Global::rsvp->getRoutingService().getUnicastRoute( destAddress, gateway );
			policyData = msg.getPolicyList().front();

			if ( senderTemplate.getSrcAddress() == NetAddress(0) 
				|| senderTemplate.getSrcAddress() == LogicalInterface::loopbackAddress 
				|| senderTemplate.getSrcAddress() == loopback ) {
				if (defaultOutLif) {
					LOG(2)( Log::API, "default out interface is", defaultOutLif->getName() );
					//senderTemplate.setSrcAddress( defaultOutLif->getLocalAddress() );
					senderTemplate.setSrcAddress( dataOutRsvpHop.getAddress());
#if !defined(BETWEEN_APIS)
				} else {
					ERROR(3)( Log::Error, "Can't find out interface for", *static_cast<SESSION_Object*>(this), "for PATH message from API" );
		// should probably return an error to the api
		return;
#endif
				}
			} else {
				if ( !RSVP_Global::rsvp->findInterfaceByAddress( senderTemplate.getSrcAddress() ) ) {
					ERROR(3)( Log::Error, "Unknown interface", senderTemplate.getSrcAddress(), "in PATH message from API" );
		// should probably return an error to the api
		return;
				}
			}
		} else if ( &hop.getLogicalInterface() == RSVP_Global::rsvp->getApiLif() ) {
			// message is from remote API -> set sender address if not set
			if ( senderTemplate.getSrcAddress() == NetAddress(0) ) {
				senderTemplate.setSrcAddress( hop.getAddress() );
			}
	       }
#if !defined(BETWEEN_APIS)
		else
#endif

		if ( destAddress.isMulticast()
			|| RSVP_Global::rsvp->findInterfaceByAddress( destAddress )
			|| RSVP_Global::rsvp->getApiServer().findApiSession( *this ) ) {
			// store multicast path messages for future API clients
			// store unicast path messages targeted to this host for future API clients
			// note that RtOutL is not emptied in 'getMulticastRoute' below
			RtOutL.insert_unique( RSVP::getApiLif() );
		}
#endif

		if ( !RSVP_Global::rsvp->findInterfaceByAddress( destAddress ) ) {
		//if (!RSVP_Global::rsvp->getApiServer().findApiSession( *this ) ){
			if ( destAddress.isMulticast() )  {
				gateway = LogicalInterface::noGatewayAddress;
				RtInIf = RSVP_Global::rsvp->getRoutingService().getMulticastRoute( senderTemplate.getSrcAddress(), destAddress, RtOutL );
				if ( !RSVP_Global::rsvp->getRoutingService().isMulticastRouter() ) {
					RtInIf = &hop.getLogicalInterface();
				}
			} else {
				RtInIf = &hop.getLogicalInterface();
				if (explicitRoute->getAbstractNodeList().front().getType() == AbstractNode::IPv4
				|| (explicitRoute->getAbstractNodeList().front().getType() == AbstractNode::UNumIfID 
				&& (explicitRoute->getAbstractNodeList().front().getInterfaceID()>>16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL))
					defaultOutLif = RSVP_Global::rsvp->getRoutingService().findOutLifByOSPF(destAddress, 0, gateway);
				else if (explicitRoute->getAbstractNodeList().front().getType() == AbstractNode::UNumIfID)
				{
					uint32 uNumIfID = explicitRoute->getAbstractNodeList().front().getInterfaceID();
					defaultOutLif = RSVP_Global::rsvp->getRoutingService().findOutLifByOSPF(destAddress, uNumIfID, gateway);
				}
				else
					defaultOutLif = RSVP_Global::rsvp->getRoutingService().getUnicastRoute( destAddress, gateway );
				if ( defaultOutLif ) RtOutL.insert_unique( defaultOutLif );
			}
		} else {
			defaultOutLif = RSVP::getApiLif();
			gateway = LogicalInterface::noGatewayAddress;
		}
	}

	//Store defaultOutLif and gateway
	outLif = defaultOutLif;
	gw = gateway;
	inLif = RtInIf;
	biDir = msg.hasUPSTREAM_LABEL_Object();

#if defined(BETWEEN_APIS)
	if ( &hop.getLogicalInterface() != RSVP::getApiLif() )
#endif
		//omitted because now it is possible that incoming and outgoing interface may be the same!
		//RtOutL.erase_key( &hop.getLogicalInterface() );

#if defined(ONEPASS_RESERVATION)
search_psb:
#endif
#if defined(WITH_API)
	bool localOnly = (!fromLocalAPI && (&hop.getLogicalInterface() != RtInIf));
#endif

#if defined(LOG_ON)
	LOG(S)( Log::Routing, "routing result after adjustment:" );
	LogicalInterfaceSet::ConstIterator logLifIter = RtOutL.begin();
	for ( ; logLifIter != RtOutL.end(); ++logLifIter ) {
		LOG(C)( Log::Routing, " " ); LOG(C)( Log::Routing, (*logLifIter)->getName() );
	}
	LOG(C)( Log::Routing, endl );
#endif

	if ( RtOutL.empty() ) {
		LOG(2)( Log::Process, "no routing information for dest:", destAddress );
	return;
	}

	PSB* cPSB = NULL;

	// Search for matching PSB, be aware of conflicting port settings; check
	// for matching PHOP address & incoming interface, as well. If any other
	// PSB is found, set its localOnly opposite to current localOnly.
	PSB_List::Iterator psbIter = RelationshipSession_PSB::followRelationship().lower_bound( &senderTemplate );
	if ( psbIter != RelationshipSession_PSB::followRelationship().end() && **psbIter == senderTemplate ) {

#if defined(WITH_API)
		do {
			if ( ( !localOnly && !(*psbIter)->isLocalOnly() )
				|| (*psbIter)->getPHopSB().checkPHOP_Data( hop, phopLIH ) ) {
				cPSB = *psbIter;
			} else if ( !localOnly ) {
				LOG(2)( Log::Process, "setting local only:", **psbIter );
				(*psbIter)->setLocalOnly( true );
			}
			++psbIter;
		}	while ( psbIter != RelationshipSession_PSB::followRelationship().end() && **psbIter == senderTemplate );
#else
		cPSB = *psbIter;
#endif
	} else {
		PSB* conflictCandidate = NULL;
		if ( senderTemplate.getLspId() == 0 ) 
		{
			if ( psbIter != RelationshipSession_PSB::followRelationship().end() ) {
				conflictCandidate = *psbIter;
			}
		} else {
			if ( psbIter != RelationshipSession_PSB::followRelationship().begin() ) {
				conflictCandidate = *(psbIter.prev());
			}
		}
		if ( conflictCandidate
			&& conflictCandidate->getSrcAddress() == senderTemplate.getSrcAddress()
			&& ( conflictCandidate->getLspId() == 0 || senderTemplate.getLspId() == 0 ) ) 
			{
			RSVP_Global::messageProcessor->sendPathErrMessage( ERROR_SPEC_Object::ConflictingSenderPorts, 0 );
	return;
		}
	}

	//Check if there is already a bidirection LSP set up. If so, just return PERR
	NetAddress upSrcAddress = getDestAddress();
	SENDER_TEMPLATE_Object upSenderTemplate(upSrcAddress, msg.getSESSION_Object().getTunnelId());
	PSB_List::Iterator psbUpIter = RelationshipSession_PSB::followRelationship().lower_bound( &upSenderTemplate );
	if ( psbUpIter != RelationshipSession_PSB::followRelationship().end() && **psbUpIter == upSenderTemplate &&
	     (*psbUpIter)->hasUPSTREAM_OUT_LABEL_Object()){
		ERROR(2)( Log::Error, "Bi-directional LSP has already been established!", *static_cast<SESSION_Object*>(this));
		RSVP_Global::messageProcessor->sendPathErrMessage( ERROR_SPEC_Object::RSVPSystemError, 0);
		return;
	}

	bool psbIsNew = ( cPSB == NULL );
	bool nonRsvp = false;
	if ( psbIsNew ) {
		cPSB = new PSB( senderTemplate );
		RelationshipSession_PSB::setRelationshipFull( this, cPSB, psbIter );
	} else {
		LOG(2)( Log::Process, "found", *cPSB );
	}

	// if the PSB is not new and the PHOP has changed -> send reservations upstream
	// if the PSB is new and because of WF, reservations already exist,
	// this is detected later in PSB::updateRoutingInfo
#if defined(ONEPASS_RESERVATINS)
	// if this a duplex PSB and a regular already exists -> ignore
	if ( !psbIsNew && RSVP_Global::messageProcessor->getOnepassPSB() ) {
		LOG(2)( Log::Process, "partially ignoring DUPLEX request, b/c original PSB exists:", *cPSB );
	goto update_basic_psb;
	}
#endif
	PHopSB* phop = findOrCreatePHopSB( hop, phopLIH );
	if ( cPSB->RelationshipPSB_PHopSB::changeRelationshipFull( cPSB, phop ) ) {
		LOG(2)( Log::Process, "setting new PHOP", *phop );
		if ( !psbIsNew ) {
#if defined(ONEPASS_RESERVATINS)
			if ( msg.getMsgType() != Message::PathResv )
#endif
			RSVP_Global::messageProcessor->markForResvRefresh( *cPSB );
		}
	}

#if defined(REFRESH_REDUCTION)
	if ( msg.hasMESSAGE_ID_Object() ) {
		cPSB->setRecvID( msg.getMESSAGE_ID_Object().getID() );
	}
#endif

	// TODO: we should check the MPLS protocol type here
	if ( msg.hasLABEL_REQUEST_Object() ) {
		if ( hop.getLogicalInterface().hasEnabledMPLS() && !getDestAddress().isMulticast()) { //&& (!fromLocalAPI) @@@@ hacked
			cPSB->setInLabelRequested();
		}
		cPSB->updateLABEL_REQUEST_Object(msg.getLABEL_REQUEST_Object());
	}
	else if ( cPSB->getInLabel() ) {
		// TODO: remove in label assignment here
	}
	if (msg.hasUPSTREAM_LABEL_Object() && msg.getUPSTREAM_LABEL_Object().getLabel()){
		// TODO: verify that the upstream label is acceptable
		if (fromLocalAPI)
			cPSB->updateUPSTREAM_IN_LABEL_Object(msg.getUPSTREAM_LABEL_Object());
		else{
			cPSB->updateUPSTREAM_OUT_LABEL_Object(msg.getUPSTREAM_LABEL_Object());
			if (!RSVP_Global::rsvp->findInterfaceByAddress(destAddress)){
				uint32 upstreamInLabel = RSVP_Global::rsvp->getMPLS().allocUpstreamInLabel();
				if (upstreamInLabel==0){
					RSVP_Global::messageProcessor->sendPathErrMessage( ERROR_SPEC_Object::RoutingProblem, ERROR_SPEC_Object::MPLSLabelAllocationFailure);
					return;
				}
				else
					cPSB->updateUPSTREAM_IN_LABEL_Object(upstreamInLabel);
			}
		}
		//Instead of creating reservation for upstream direction NOW, as per RFC3473, Section 3.1
		//the reservation is done when upstream RESV is received
	}
	if (msg.hasSUGGESTED_LABEL_Object()){
		cPSB->updateSUGGESTED_LABEL_Object(msg.getSUGGESTED_LABEL_Object());
	}
	if (msg.hasSESSION_ATTRIBUTE_Object()){
		cPSB->updateSESSION_ATTRIBUTE_Object(msg.getSESSION_ATTRIBUTE_Object());
	}
	if ( cPSB->updateEXPLICIT_ROUTE_Object( explicitRoute ) ) Path_Refresh_Needed = true;
	cPSB->setDataChannelInfo(vLSRoute, dataInRsvpHop, dataOutRsvpHop);
	LABEL_SET_Object* labelSetObj = const_cast<LABEL_SET_Object*>(msg.getLABEL_SET_Object());
	if (labelSetObj) cPSB->updateLABEL_SET_Object(labelSetObj->borrow());

	//hacks @@@@
	if (((Message*)&msg)->getDRAGON_UNI_Object() != NULL)
		cPSB->updateDRAGON_UNI_Object(((Message*)&msg)->getDRAGON_UNI_Object());

	// update PSB
	if ( cPSB->updateSENDER_TSPEC_Object( msg.getSENDER_TSPEC_Object() ) ) {
		Path_Refresh_Needed = true;
	}

#if defined(ONEPASS_RESERVATION)
	if ( msg.getMsgType() == Message::PathResv && msg.hasDUPLEX_Object() ) {
		if ( cPSB->updateDuplexObject( msg.getDUPLEX_Object() ) ) {
			Path_Refresh_Needed = true;
		}
		RSVP_Global::messageProcessor->setOnepassPSB( cPSB );
	} else if ( RSVP_Global::messageProcessor->getOnepassPSB() ) {
		cPSB->setTTL( 0 );
		RSVP_Global::messageProcessor->getOnepassPSB()->setDuplexPSB( cPSB );
goto update_basic_psb;
	} else {
		if ( cPSB->clearDuplexObject() ) Path_Refresh_Needed = true;
		cPSB->setDuplexPSB( NULL );
	}
#endif

	// check for non-RSVP clouds
	if ( TTL != msg.getTTL() ) {
		LOG(2)( Log::Process, "detected TTL mismatch, packet ttl:", (uint32)TTL );
#if defined(WITH_API)
		if ( &hop.getLogicalInterface() != RSVP_Global::rsvp->getApiLif() )
#endif
			nonRsvp = true;
	}
	cPSB->setTTL( TTL );

#if defined(WITH_API)
	if ( (&hop.getLogicalInterface() == RSVP_Global::rsvp->getApiLif()) && !msg.getADSPEC_Object() ) {
		const ADSPEC_Object* msgAdspec = new ADSPEC_Object( 0, ieee32floatInfinite, 0, sint32Infinite );
		cPSB->updateADSPEC_Object( msgAdspec, nonRsvp );
		msgAdspec->destroy();
	} else
#endif
		cPSB->updateADSPEC_Object( msg.getADSPEC_Object(), nonRsvp );

	cPSB->updateUnknownObjectList( msg.getUnknownObjectList() );

#if defined(WITH_API)
	if (localOnly) {
		LOG(2)( Log::Process, "setting local only:", *cPSB );
		cPSB->setLocalOnly(true);
	}
	cPSB->setFromAPI( &hop.getLogicalInterface() == RSVP_Global::rsvp->getApiLif() );
#endif

#if defined(ONEPASS_RESERVATION)
update_basic_psb:
#endif

	cPSB->setTimeoutTime( msg.getTIME_VALUES_Object().getRefreshPeriod() );

	// update outgoing interfaces of PSB and maintain relations
	// between PSBs, RSBs, and OutISBs; update TC, if necessary
#if defined(ONEPASS_RESERVATION)
	if ( msg.getMsgType() == Message::PathResv ) {
		if ( ( style != SE || style != WF ) ) {
			style = FF;
		} else {
			LOG(2)( Log::Error, "style ERROR:", style );
			// TODO: ERROR
		}
		cPSB->updateRoutingInfo( RtOutL, gateway, Path_Refresh_Needed, false, msg.getMsgType() == Message::PathResv, msg.getTIME_VALUES_Object().getRefreshPeriod() );
	} else
#endif
	cPSB->updateRoutingInfo( RtOutL, gateway, Path_Refresh_Needed, false );
}

void Session::processAsyncRoutingEvent( const NetAddress& src, const LogicalInterface& inLif, LogicalInterfaceSet& lifList ) {
	SENDER_TEMPLATE_Object senderTemplate( src, 0 );
	PSB_List::Iterator psbIter = RelationshipSession_PSB::followRelationship().lower_bound( &senderTemplate );
	for ( ; psbIter != RelationshipSession_PSB::followRelationship().end() && (*psbIter)->getSrcAddress() == src; ++psbIter ) {
		if ( &(*psbIter)->getPHopSB().getHop().getLogicalInterface() == &inLif ) {
#if defined(WITH_API)
			if ( !(*psbIter)->isFromAPI() ) {
				lifList.insert_unique( RSVP::getApiLif() );
			} else {
				lifList.erase_key( RSVP::getApiLif() );
			}
#endif
			if ( !(*psbIter)->getEXPLICIT_ROUTE_Object() )
			(*psbIter)->updateRoutingInfo( lifList, LogicalInterface::noGatewayAddress, false, true );
		}
	}
}

void Session::processAsyncRoutingEvent( const LogicalInterface& outLif, const NetAddress& gateway ) {
	LogicalInterfaceSet lifList;
	lifList.insert_unique( &outLif );
	PSB_List::Iterator psbIter = RelationshipSession_PSB::followRelationship().begin();
	for ( ; psbIter != RelationshipSession_PSB::followRelationship().end(); ++psbIter ) {
		if ( &(*psbIter)->getPHopSB().getHop().getLogicalInterface() != &outLif ) {
			(*psbIter)->updateRoutingInfo( lifList, gateway, false, true );
		}
	}
}

void Session::processPTEAR( const Message& msg, const PacketHeader& hdr, const LogicalInterface& inlif ) {
	const SENDER_Object& sender = msg.getSENDER_TEMPLATE_Object();
	PSB_List::Iterator psbIter = RelationshipSession_PSB::followRelationship().find( const_cast<SENDER_Object*>(&sender) );
	for ( ; psbIter != RelationshipSession_PSB::followRelationship().end() && **psbIter == sender; ++psbIter ) {
		Hop* hop = RSVP_Global::rsvp->findHop( inlif, hdr.getSrcAddress());
		//Hop* hop = RSVP_Global::rsvp->findHop( inlif, msg.getRSVP_HOP_Object().getAddress() );
		if ( hop && (*psbIter)->getPHopSB().checkPHOP_Data( *hop, msg.getRSVP_HOP_Object().getLIH() ) ) {
			break;
		}
	}
	if ( psbIter == RelationshipSession_PSB::followRelationship().end() || **psbIter != sender ) {
		LOG(1)( Log::Process, "no PSB found -> ignoring PTEAR message" );
		return;
	}
	ieee32float bandwidth = ((const TSpec &)(*psbIter)->getSENDER_TSPEC_Object()).get_r();
	LogicalInterfaceSet::ConstIterator lifIter = (*psbIter)->getOutLifSet().begin();
	for ( ;lifIter != (*psbIter)->getOutLifSet().end(); ++lifIter ) {
		const_cast<Message&>(msg).clearRSVP_HOP_Object(msg.getRSVP_HOP_Object());
		const_cast<Message&>(msg).setRSVP_HOP_Object( (*psbIter)->getDataOutRsvpHop() );
		// unicast only -> no need to reset EXPLICIT_ROUTE_Object in message
		if ( (*psbIter)->getEXPLICIT_ROUTE_Object() ) {
			if ((*psbIter)->getGateway() != LogicalInterface::noGatewayAddress)
				(*lifIter)->sendMessage( msg, (*psbIter)->getGateway());
			else
				(*lifIter)->sendMessage( msg, (*psbIter)->getEXPLICIT_ROUTE_Object()->getAbstractNodeList().front().getAddress(), sender.getSrcAddress(), (*psbIter)->getGateway() );
		} else
			(*lifIter)->sendMessage( msg, getDestAddress(), sender.getSrcAddress(), (*psbIter)->getGateway() );
	}
#if defined(ONEPASS_RESERVATION)
	if ( (*psbIter)->isOnepass() && (*psbIter)->getPHopSB().getPSB_List().size() > 1 ) {
		PHopSB& phop = (*psbIter)->getPHopSB();
		delete *psbIter;
		RSVP_Global::messageProcessor->internalResvRefresh( this, phop );
	} else
	{
#endif
	//@@@@
	//if (outLif && outLif != RSVP_Global::rsvp->getApiLif()){
	//	RSVP_Global::rsvp->getRoutingService().notifyOSPF(msg.getMsgType(), outLif->getAddress(), bandwidth);
	//}
	//if (inLif && inLif != RSVP_Global::rsvp->getApiLif() && biDir){
	//	RSVP_Global::rsvp->getRoutingService().notifyOSPF(msg.getMsgType(), inLif->getAddress(), bandwidth);
	//}
#if defined(ONEPASS_RESERVATION)
	}
#endif
	delete *psbIter;
}

void Session::processPERR( Message& msg, const LogicalInterface& inLif ) {
	const SENDER_Object& sender = msg.getSENDER_TEMPLATE_Object();
	PSB_List::ConstIterator psbIter = RelationshipSession_PSB::followRelationship().find( const_cast<SENDER_Object*>(&sender)  );
	if ( psbIter == RelationshipSession_PSB::followRelationship().end() || **psbIter != sender ) {
		LOG(1)( Log::Process, "no PSB found -> ignoring PERR message" );
		return;
	}
	const LogicalInterface& sendLif = (*psbIter)->getPHopSB().getHop().getLogicalInterface();
	msg.setTTL( 63 );
	if ( &sendLif != &inLif ) {
		NetAddress peer;
		RSVP_Global::rsvp->getRoutingService().getPeerIPAddr(sendLif.getAddress(), peer);
		sendLif.sendMessage( msg, peer );
	}
}

inline ERROR_SPEC_Object::ErrorCode Session::processRESV_FDesc( const FLOWSPEC_Object& flowspec,
	const FilterSpecList& filterList, const Message& msg, Hop& nhop ) {
	RSB* currentRSB = NULL;
	OutISB* currentOutISB = NULL;
	RSB_Contents* oldRSB = NULL;
	PSB_List matchingPSB_List;
	bool tear = (msg.getMsgType() == Message::ResvTear);
	FilterStyle msgStyle = msg.getSTYLE_Object().getStyle();
	TrafficControl::UpdateResult tcResult = { false, ERROR_SPEC_Object::Confirmation };

	// search matching PSBs
#if defined(USE_SCOPE_OBJECT)
	if ( style == WF && msg.getSCOPE_Object() ) {
		matchPSBsAndSCOPEandOutInterface( msg.getSCOPE_Object()->getAddressList(),
			nhop.getLogicalInterface(), matchingPSB_List, currentOutISB );
		if ( matchingPSB_List.empty() ) {
			LOG(1)( Log::Process, "no PSB for SCOPE list found -> ignoring RESV/RTEAR" );
	return ERROR_SPEC_Object::Confirmation;
		}
	} else
#endif
	matchPSBsAndFiltersAndOutInterface( filterList, nhop.getLogicalInterface(), matchingPSB_List, currentOutISB );

	// check existence of matching PSBs
	if ( matchingPSB_List.empty() ) return ERROR_SPEC_Object::NoSenderInformation;

	if ( !tear ) {                              // **** RESV processing ****

#if defined(ONEPASS_RESERVATION)
		// check for old onepass RSB and delete it later
		bool weakRSB = ( currentOutISB && currentOutISB->getRSB_List().front()->getNextHop().getAddress() == NetAddress(0) );
#endif

		// find RSB at OutISB
		RSB_List::ConstIterator rsbIter;
		if ( !currentOutISB ) {
			currentOutISB = nhop.getLogicalInterface().getTC().createOutISB( nhop.getLogicalInterface() );
			rsbIter = currentOutISB->getRSB_List().begin();
		} else {
			RSB_Key key( nhop );
			rsbIter = currentOutISB->getRSB_List().lower_bound( &key );
#if defined(WITH_API)
			if ( &nhop.getLogicalInterface() == RSVP_Global::rsvp->getApiLif() ) {
				for ( ; rsbIter != currentOutISB->getRSB_List().end() && **rsbIter == key; ++rsbIter ) {
					if ( (*rsbIter)->getAPI_Port() == msg.getRSVP_HOP_Object().getLIH() ) 
		{
			break;
					}
				}
			}
#endif
			if ( rsbIter != currentOutISB->getRSB_List().end() && **rsbIter == key ) {
				currentRSB = *rsbIter;
			}
		}

		// change or create RSB
		if ( currentRSB != NULL ) {
			LOG(2)( Log::Process, "found RSB:", *currentRSB );
			oldRSB = currentRSB->createBackup();
			if ( rsbCount == 1 ) style = msgStyle;
		} else {
			// rest of style checking: if another RSB exists and differs from RESV
			// -> send ERROR message
			if ( rsbCount == 1 && style != msgStyle ) {
				if ( currentOutISB->getRSB_List().empty() ) delete currentOutISB;
	return ERROR_SPEC_Object::ConflictingReservationStyle;
			} else {
				style = msgStyle;
			}
			currentRSB = new RSB( nhop );
			rsbCount += 1;
			currentOutISB->RelationshipOutISB_RSB::setRelationshipFull( currentOutISB, currentRSB, rsbIter );
#if defined(WITH_API)
			if ( &nhop.getLogicalInterface() == RSVP_Global::rsvp->getApiLif() ) {
				currentRSB->setAPI_Port( msg.getRSVP_HOP_Object().getLIH() );
			}
#endif
		}

#if defined(REFRESH_REDUCTION)
		if ( msg.hasMESSAGE_ID_Object() ) {
			currentRSB->setRecvID( msg.getMESSAGE_ID_Object().getID() );
		}
#endif

#if defined(WITH_API)
		const bool removeOldFilters = ( &nhop.getLogicalInterface() == RSVP_Global::rsvp->getApiLif() );
#else
		const bool removeOldFilters = false;
#endif

		// in case of SE, new and known filters are recorded in
		// 'RSB::setFiltersAndTimeout' and used for selective refresh and confirm
		PSB_List* newFilterList = NULL;
		FilterSpecList* knownFilterList = NULL;
		if ( style == SE ) {
			newFilterList = new PSB_List;
			knownFilterList = new FilterSpecList;
		}

		// update flowspec & filters
		// update filters -> sets relationsship of OutISBs, as well
		bool filterChange = currentRSB->setFiltersAndTimeout( matchingPSB_List, msg.getTIME_VALUES_Object().getRefreshPeriod(), removeOldFilters, newFilterList, knownFilterList );
		bool flowspecChange = currentRSB->updateContents( flowspec, msg.getSCOPE_Object() );
		bool confirmImmediately = false;

		// new RSB -> might change state in NBMA networks or in policy control
		if ( filterChange || flowspecChange ) {

			TrafficControl::UpdateFlag uflag = TrafficControl::ModifiedRSB;
			if (!oldRSB) {
				uflag = TrafficControl::NewRSB;
			}

			uint8 inPlace = 0;
			if ( currentOutISB->getForwardFlowspec() != *RSVP::zeroFlowspec ) {
				inPlace = ERROR_SPEC_Object::InPlace;
			}

			tcResult = currentOutISB->getOI().getTC().updateTC( *currentOutISB,
				flowspecChange ? currentRSB : NULL, uflag );

#if defined(ONEPASS_RESERVATION)
			if ( weakRSB && tcResult.error == ERROR_SPEC_Object::Confirmation ) {
				tcResult.changed = true;
			}
#endif

			if ( tcResult.error != ERROR_SPEC_Object::Confirmation ) {
				if ( oldRSB ) {
					currentRSB->replaceWithBackup( *oldRSB, msg.getTIME_VALUES_Object().getRefreshPeriod() );
					oldRSB = NULL;
				} else {
					// remove filters now; relationship to OutISB is needed in 'setFiltersAndTimeout'
					currentRSB->setFiltersAndTimeout( PSB_List(), 0, true );
					// remove relationship to avoid additional TC update
					currentRSB->RelationshipRSB_OutISB::clearRelationshipFull();
					// if this is the last RSB, that might remove OutISB, as well
					delete currentRSB;
					rsbCount -= 1;
				}
			} else if ( tcResult.changed ) {
				PSB_List::ConstIterator psbIter = matchingPSB_List.begin();
				for ( ; psbIter != matchingPSB_List.end(); ++psbIter ) {
					RSVP_Global::messageProcessor->markForResvRefresh( **psbIter );
				}
			} else if ( style == SE ) {
				PSB_List::ConstIterator psbIter = newFilterList->begin();
				for ( ; psbIter != newFilterList->end(); ++psbIter ) {
					RSVP_Global::messageProcessor->markForResvRefresh( **psbIter );
				}
				if ( !knownFilterList->empty() ) confirmImmediately = true;
			} else {
				confirmImmediately = true;
			}
#if defined(ONEPASS_RESERVATION) 
			if ( tcResult.error == ERROR_SPEC_Object::Confirmation ) {
				currentOutISB->updateOnepassFlags();
			}
#endif
		} else {
			confirmImmediately = true;
			LOG(1)( Log::Process, "no TC update for flow descriptor" );
		}
		if ( msg.hasRESV_CONFIRM_Object() ) {
			if ( confirmImmediately ) {
				if ( style == SE ) {
					if ( !knownFilterList->empty() ) {
						RSVP_Global::messageProcessor->addToConfirmMsg( flowspec, *knownFilterList );
					}
					if ( !newFilterList->empty() ) {
						RSVP_Global::messageProcessor->setConfirmOutISB( *currentOutISB );
					}
				} else {
					RSVP_Global::messageProcessor->addToConfirmMsg( flowspec, filterList );
				}
			} else {
				RSVP_Global::messageProcessor->setConfirmOutISB( *currentOutISB );
			}
		}
		if (oldRSB) delete oldRSB;
		if ( style == SE ) {
			delete newFilterList;
			delete knownFilterList;
		}

	} else {                                  // **** RTEAR processing ****
		if ( currentOutISB ) {
			RSB_Key key( nhop );
			RSB_List::ConstIterator rsbIter = currentOutISB->getRSB_List().find( &key );
			if ( rsbIter != currentOutISB->getRSB_List().end() ) {
				if ( style == WF || (*rsbIter)->teardownFilters( filterList ) ) {
					delete *rsbIter;
					rsbCount -= 1;
				} else {
					nhop.getLogicalInterface().getTC().updateTC( *currentOutISB, *rsbIter, TrafficControl::ModifiedRSB );
				}
	return ERROR_SPEC_Object::Confirmation;
			}
		}
		LOG(1)( Log::Process, "ignoring RTEAR flow descriptor" );
	} // RESV vs. RTEAR
	return tcResult.error;
}

void Session::processRESV( const Message& msg, Hop& nhop ) {

	// check style; special case of rsbCount == 1 is in processRESV_FDesc
	FilterStyle msgStyle = msg.getSTYLE_Object().getStyle();
	if (msgStyle != FF )  //Currently, only FF is supported, SE & WF are not implemented
	{
		RSVP_Global::messageProcessor->sendResvErrMessage( 0, ERROR_SPEC_Object::UnknownReservationStyle, 0 );
	return;
	} else if ( rsbCount > 1 && style != msgStyle ) {
		RSVP_Global::messageProcessor->sendResvErrMessage( 0, ERROR_SPEC_Object::ConflictingReservationStyle, 0 );
	return;
	} else if ( rsbCount == 0 ) {
		style = msgStyle;
	}

	// number of flow descriptors in combination with filter style is already
	// checked in Message class

	// loop through all flow descriptors and process each one independently
	const FlowDescriptorList& flowDescriptorList = msg.getFlowDescriptorList();
	FlowDescriptorList::ConstIterator flowdescIter = flowDescriptorList.begin();
	for ( ; flowdescIter != flowDescriptorList.end() ; ++flowdescIter ) {
		LOG(2)( Log::Process, "processing flow descriptor", *flowdescIter );
		ieee32float bandwidth = ((*flowdescIter).getFlowspec())? ((*flowdescIter).getFlowspec())->getEffectiveRate():0;
		ERROR_SPEC_Object::ErrorCode result = processRESV_FDesc( *(*flowdescIter).getFlowspec(), (*flowdescIter).filterSpecList, msg, nhop );
		if ( result != ERROR_SPEC_Object::Confirmation ) {
			if ( msg.getMsgType() != Message::ResvTear ) {
				RSVP_Global::messageProcessor->sendResvErrMessage( 0, result, 0, *flowdescIter );
			}
		}
		else{  //Send notification to OSPF
			//@@@@hack
			//if (outLif && outLif != RSVP_Global::rsvp->getApiLif()){
			//	RSVP_Global::rsvp->getRoutingService().notifyOSPF(msg.getMsgType(), outLif->getAddress(), bandwidth);
			//}
			//if (inLif && inLif != RSVP_Global::rsvp->getApiLif() && biDir){
			//	RSVP_Global::rsvp->getRoutingService().notifyOSPF(msg.getMsgType(), inLif->getAddress(), bandwidth);
			//}
		}
	} // loop through flow descriptors
}

void Session::processRERR( Message& msg, Hop& hop ) {
                              assert( msg.getFlowDescriptorList().size() == 1 );

	const FLOWSPEC_Object& flowspec = *msg.getFlowDescriptorList().front().getFlowspec();
	const ERROR_SPEC_Object& error = msg.getERROR_SPEC_Object();
	bool acFailure = (error.getCode() == ERROR_SPEC_Object::AdmissionControlFailure);
	ieee32float bandwidth = flowspec.getEffectiveRate();

	// find PHop and relevant PSBs
	PHopSBKey key( hop, msg.getRSVP_HOP_Object().getLIH() );
	PHOP_List::Iterator pIter = RelationshipSession_PHopSB::followRelationship().find( &key );
	if ( pIter == RelationshipSession_PHopSB::followRelationship().end() ) {
		LOG(3)( Log::Process, "no PHOP state found for:", msg.getRSVP_HOP_Object().getAddress(), "-> ignoring RERR" );
	return;
	}
	PSB_List matchingPSB_List;
	if ( style == WF ) {
		matchingPSB_List = (*pIter)->getPSB_List();
	} else {
		(*pIter)->matchPSBsAndFilters( msg.getFlowDescriptorList().front().filterSpecList, matchingPSB_List );
	}

	// collect affected OutISBs, set blockade at PSBs if admission control error
	OutISB** outArray = new OutISB*[RSVP_Global::rsvp->getInterfaceCount()];
	for ( uint32 i = 0; i < RSVP_Global::rsvp->getInterfaceCount(); ++i ) {
		outArray[i] = NULL;
	}
	PSB_List::Iterator psbIter = matchingPSB_List.begin();
	for ( ; psbIter != matchingPSB_List.end(); ++psbIter ) {
		if ( acFailure ) {
			(*psbIter)->setBlockade( flowspec, hop.getLogicalInterface().getRefreshInterval().tv_sec );
		}
		uint32 i;
		for ( i = 0; i < RSVP_Global::rsvp->getInterfaceCount(); ++i ) {
			if ( i != hop.getLIH() && !outArray[i] ) {
				outArray[i] = (*psbIter)->getOutISB(i);
			}
		}
	}

	// find RSBs via OutISBs for all interfaces and forward RERR message
	msg.setTTL( 63 );
	uint32 i;
	for ( i = 0; i < RSVP_Global::rsvp->getInterfaceCount(); ++i ) {
		OutISB* oisb = outArray[i];
		if ( oisb ) {
			const RSB_List& rsbList = oisb->getRSB_List();
			RSB_List::ConstIterator rsbIter = rsbList.begin();
			for ( ; rsbIter != rsbList.end(); ++rsbIter ) {
				const RSB* rsb = *rsbIter;
				const LogicalInterface& sendLif = rsb->RelationshipRSB_OutISB::followRelationship()->getOI();
				if ( acFailure && (error.getFlags() & ERROR_SPEC_Object::InPlace)
					&& rsb->getFLOWSPEC_Object() < flowspec ) {
			continue;
				}
				PSB_List::Iterator psbIter = matchingPSB_List.begin();
				msg.clearRSVP_HOP_Object(msg.getRSVP_HOP_Object());
				msg.setRSVP_HOP_Object( (*psbIter)->getDataOutRsvpHop() );
#if defined(WITH_API)
				if ( acFailure && &sendLif == RSVP_Global::rsvp->getApiLif()
					&& rsb->getFLOWSPEC_Object() < flowspec ) {
					const_cast<ERROR_SPEC_Object&>(error).addFlag( ERROR_SPEC_Object::NotGuilty );
					sendLif.sendMessage( msg, rsb->getNextHop().getAddress() );
					const_cast<ERROR_SPEC_Object&>(error).removeFlag( ERROR_SPEC_Object::NotGuilty );
				} else
#endif
					sendLif.sendMessage( msg, rsb->getNextHop().getAddress() );
				//@@@@hack
				//if (outLif && outLif != RSVP_Global::rsvp->getApiLif()){
				//	RSVP_Global::rsvp->getRoutingService().notifyOSPF(msg.getMsgType(), outLif->getAddress(), bandwidth);
				//}
				//if (inLif && inLif != RSVP_Global::rsvp->getApiLif() && biDir){
				//	RSVP_Global::rsvp->getRoutingService().notifyOSPF(msg.getMsgType(), inLif->getAddress(), bandwidth);
				//}

			}
		}
	}
	// flowspec is now blockaded at PSBs -> initiate refresh sequence
	if ( acFailure ) {
		RSVP_Global::messageProcessor->internalResvRefresh( this, **pIter );
	}
	delete [] outArray;
}

#if defined(WITH_API)
bool Session::deregisterAPI( const NetAddress& address, uint16 port ) {
	bool sessionDeleted = false;
	PSB_List::Iterator psbIter = RelationshipSession_PSB::followRelationship().begin();
	while ( !sessionDeleted && psbIter != RelationshipSession_PSB::followRelationship().end() ) {
		PSB* psb = *psbIter;
		++psbIter;
		if ( psb->getAPI_Port() == port && psb->getAPI_Address() == address ) {
			sessionDeleted = (RelationshipSession_PSB::followRelationship().size() == 1);
			psb->sendTearMessage();
			ieee32float bandwidth = psb->getSENDER_TSPEC_Object().get_r();
			//@@@@hack
			//if (outLif && outLif != RSVP_Global::rsvp->getApiLif()){
			//	RSVP_Global::rsvp->getRoutingService().notifyOSPF(Message::PathTear, outLif->getAddress(), bandwidth	);
			//}
			//if (inLif && inLif != RSVP_Global::rsvp->getApiLif() && biDir){
			//	RSVP_Global::rsvp->getRoutingService().notifyOSPF(Message::PathTear, inLif->getAddress(), bandwidth);
			//}
			
 			delete psb;
		} else if ( psb->getForwardFlowspec() != NULL && psb->getOutISB(RSVP::getApiLif()->getLIH()) ) {
			bool oisbDeleted = false;
			RSB_List& rsbList = psb->getOutISB(RSVP::getApiLif()->getLIH())->getRSB_List();
			RSB_List::ConstIterator rsbIter = rsbList.begin();
			while ( !oisbDeleted && rsbIter != rsbList.end() ) {
				RSB* rsb = *rsbIter;
				++rsbIter;
				if ( rsb->getAPI_Port() == port && rsb->getAPI_Address() == address ) {
					oisbDeleted = (rsbList.size() == 1);
					delete rsb;
					rsbCount -= 1;
				}
			}
		}
	}
	return sessionDeleted;
}

void Session::registerAPI() {
	PSB_List::Iterator psbIter = RelationshipSession_PSB::followRelationship().begin();
	for (; psbIter != RelationshipSession_PSB::followRelationship().end(); ++psbIter ) {
		if ( !(*psbIter)->isFromAPI() ) {
			(*psbIter)->sendRefresh( *RSVP::getApiLif() );
		}
	}
}
#endif
