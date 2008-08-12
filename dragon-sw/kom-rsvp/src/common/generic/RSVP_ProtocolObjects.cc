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
#include "RSVP_ProtocolObjects.h"

INetworkBuffer& operator>> ( INetworkBuffer& buffer, SESSION_Object& o ) {
	uint16 dummy;
	buffer >> o.tunnelAddress >> dummy >> o.tunnelID >> o.extendedTunnelID;
  return buffer;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const SESSION_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::SESSION, 7);
	buffer << o.tunnelAddress << (uint16)0 << o.tunnelID << o.extendedTunnelID;
	return buffer;
}

ostream& operator<< ( ostream& os, const SESSION_Object& o ) {
	os << o.tunnelAddress << "/" << o.tunnelID << "/" << o.extendedTunnelID;
	return os;
}

void SESSION_ATTRIBUTE_Object::readFromBuffer(INetworkBuffer& buffer, uint16 len, uint8 C_Type)
{
	switch (C_Type){
		case 1:  buffer >> excludeAny >> includeAny >> includeAll; break;
		case 7:  break;
		default: buffer.skip(len); return;
	}	
	buffer >> setupPri >> holdingPri >> flags >> nameLength;
	String ssName;
	char s;
	uint8 ui8 = 0;
	for (int i = 0; i < nameLength; i++){
		buffer >> ui8;
		s = ui8; 
		if (s!=0) ssName += String(s);
	}
	sessionName = ssName;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const SESSION_ATTRIBUTE_Object& o ) {
	if (o.hasRA()){
		buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::SESSION_ATTRIBUTE, 1);
		buffer << o.excludeAny << o.includeAny << o.includeAll;
	}
	else
		buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::SESSION_ATTRIBUTE, 7);
	buffer << o.setupPri << o.holdingPri << o.flags << o.nameLength;
	for (uint32 i = 0; i < o.sessionName.length(); i++) buffer << (uint8)o.sessionName[i];
	for (uint32 j = 0; j<o.nameLength-o.sessionName.length(); j++)  buffer << (uint8)0;
	return buffer;
}

ostream& operator<< ( ostream& os, const SESSION_ATTRIBUTE_Object& o ) {
	if (o.hasRA())
		os << "ExcludeAny: " << o.excludeAny << " IncludeAny: " << o.includeAny << " IncludeAll: " << o.includeAll;
	os << " SetupPriority: " << (uint32)o.setupPri << " HoldingPriority: " << (uint32)o.holdingPri << " Flags: " << (uint32)o.flags << " NameLength: " << (uint32)o.nameLength;
	os << " SessionName: " << o.sessionName;
	return os;
}

INetworkBuffer& operator>> ( INetworkBuffer& buffer, RSVP_HOP_TLV_SUB_Object& o ) {
	buffer >> o.type >> o.length;
	switch (o.type){
		case RSVP_HOP_TLV_SUB_Object::IPv4:
			buffer >> o.value.ip4;
			break;
		case RSVP_HOP_TLV_SUB_Object::IfIndex:
			buffer >> o.value.ifIndex.addr >> o.value.ifIndex.interfaceID;
			break;
		default:
			buffer.skip(o.length-4);
			break;		
	}
	return buffer;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const RSVP_HOP_TLV_SUB_Object& o ) {
	buffer << o.type << o.length;
	switch (o.type){
		case RSVP_HOP_TLV_SUB_Object::IPv4:
			buffer << o.value.ip4;
			break;
		case RSVP_HOP_TLV_SUB_Object::IfIndex:
			buffer << o.value.ifIndex.addr << o.value.ifIndex.interfaceID;
			break;
		default:
			break;		
	}
	
  return buffer;
}

ostream& operator<< ( ostream& os, const RSVP_HOP_TLV_SUB_Object& o ) {
	os << " TLV: " << " Type: " << o.type  << " Length: " << o.length << " Value: ";
	switch (o.type){
		case RSVP_HOP_TLV_SUB_Object::IPv4:
			os << "IPv4: " << o.value.ip4;
			break;
		case RSVP_HOP_TLV_SUB_Object::IfIndex:
			os << "IfIndex Addr: " << o.value.ifIndex.addr << " IfID: " << o.value.ifIndex.interfaceID;
			break;
		default:
			break;		
	}
	return os;
}

void RSVP_HOP_Object::readFromBuffer(INetworkBuffer& buffer, uint16 len, uint8 C_Type)
{
        const RSVP_HOP_TLV_SUB_Object &t = this->getTLV();
	buffer >> hopAddress >> LIH;
	switch (C_Type){
		case 1:  break;
		case 3:	buffer >> *(RSVP_HOP_TLV_SUB_Object*)&t; break;
		default: buffer.skip(len); break;
	}	
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const RSVP_HOP_Object& o ) {
	if (o.tlv.getType()!=RSVP_HOP_TLV_SUB_Object::Illegal)
		buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::RSVP_HOP, 3 );
	else
		buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::RSVP_HOP, 1 );
	buffer << o.hopAddress << o.LIH;
	if (o.tlv.getType()!=RSVP_HOP_TLV_SUB_Object::Illegal)
	{
		buffer << o.getTLV();
	}
 return buffer;
}

ostream& operator<< ( ostream& os, const RSVP_HOP_Object& o ) {
	os << "Hop address: " << o.getAddress() << "LIH: " << (uint32)o.getLIH();
	if (o.tlv.getType()!=RSVP_HOP_TLV_SUB_Object::Illegal)
		os << o.getTLV();
	return os;
}

INetworkBuffer& operator>> ( INetworkBuffer& buffer, TIME_VALUES_Object& o ) {
	buffer >> o.refreshPeriod;
	return buffer;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const TIME_VALUES_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::TIME_VALUES, 1 );
	buffer << o.refreshPeriod;
  return buffer;
}

ostream& operator<< ( ostream& os, const TIME_VALUES_Object& o ) {
	os << (uint32)o.refreshPeriod;
	return os;
}

INetworkBuffer& operator>> ( INetworkBuffer& buffer, ERROR_SPEC_Object& o ) {
	buffer >> o.nodeAddress >> o.flags >> o.errorCode >> o.errorValue;
	return buffer;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const ERROR_SPEC_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::ERROR_SPEC, 1 );
	buffer << o.nodeAddress << o.flags << o.errorCode << o.errorValue;
  return buffer;
}

ostream& operator<< ( ostream& os, const ERROR_SPEC_Object& o ) {
	os << o.nodeAddress << " ";
	switch (o.errorCode) {
		case ERROR_SPEC_Object::Confirmation: os << "Confirmation:" << o.errorValue; break;
		case ERROR_SPEC_Object::AdmissionControlFailure: os << "AdmissionControlFailure:" << o.errorValue; break;
		case ERROR_SPEC_Object::PolicyControlFailure: os << "PolicyControlFailure:" << o.errorValue; break;
		case ERROR_SPEC_Object::NoPathInformation: os << "NoPathInformation:" << o.errorValue; break;
		case ERROR_SPEC_Object::NoSenderInformation: os << "NoSenderInformation:" << o.errorValue; break;
		case ERROR_SPEC_Object::ConflictingReservationStyle: os << "ConflictingReservationStyle:" << o.errorValue; break;
		case ERROR_SPEC_Object::UnknownReservationStyle: os << "UnknownReservationStyle:" << o.errorValue; break;
		case ERROR_SPEC_Object::ConflictingDestPorts: os << "ConflictingDestPorts:" << o.errorValue; break;
		case ERROR_SPEC_Object::ConflictingSenderPorts: os << "ConflictingSenderPorts:" << o.errorValue; break;
		case ERROR_SPEC_Object::ServicePreempted: os << "ServicePreempted:" << o.errorValue; break;
		case ERROR_SPEC_Object::UnknownObjectClass: os << "UnknownObjectClass:" << o.errorValue; break;
		case ERROR_SPEC_Object::UnknownObjectCType: os << "UnknownObjectCType:" << o.errorValue; break;
		case ERROR_SPEC_Object::ErrorAPI: os << "ErrorAPI" << o.errorValue << o.errorValue; break;
		case ERROR_SPEC_Object::TrafficControlError: os << "TrafficControlError:" << o.errorValue; break;
		case ERROR_SPEC_Object::TrafficControlSystemError: os << "TrafficControlSystemError:" << o.errorValue; break;
		case ERROR_SPEC_Object::RSVPSystemError: os << "RSVPSystemError:" << o.errorValue; break;
		case ERROR_SPEC_Object::RoutingProblem: os << "RoutingProblem:";
								switch (o.errorValue){
									case ERROR_SPEC_Object::BadExplicitRoute: os << " BadExplicitRoute "; break;
									case ERROR_SPEC_Object::BadStrictNode: os << " BadStrictNode "; break;
									case ERROR_SPEC_Object::BadLooseNode: os << " BadLooseNode "; break;
									case ERROR_SPEC_Object::BadInitiaSubobject: os << " BadInitiaSubobject "; break;
									case ERROR_SPEC_Object::NoRouteAvailToDest: os << " NoRouteAvailToDest "; break;
									case ERROR_SPEC_Object::UnacceptableLabel: os << " UnacceptableLabel "; break;
									case ERROR_SPEC_Object::RROIndicatedRoutingLoops: os << " RROIndicatedRoutingLoops "; break;
									case ERROR_SPEC_Object::NonRSVPCapableRouterPresent: os << " NonRSVPCapableRouterPresent "; break;
									case ERROR_SPEC_Object::MPLSLabelAllocationFailure: os << " MPLSLabelAllocationFailure "; break;
									case ERROR_SPEC_Object::UnsupportedL3PID: os << " UnsupportedL3PID "; break;
									default: os << o.errorValue; break;
								}
								break;
		case ERROR_SPEC_Object::Notify: os << "Notify:";
								switch (o.errorValue){
									case ERROR_SPEC_Object::RROtooLargeForMTU: os << " RROtooLargeForMTU "; break;
									case ERROR_SPEC_Object::RRONotification: os << " RRONotification "; break;
									case ERROR_SPEC_Object::TunnelLocallyRepaired: os << " TunnelLocallyRepaired "; break;
									default: os << o.errorValue; break;
								}
								break;
		default: os << "unknown Error:" << o.errorValue; break;
	};
	switch (o.flags) {
		case ERROR_SPEC_Object::InPlace: os << " InPlace "; break;
		case ERROR_SPEC_Object::NotGuilty: os << " NotGuilty "; break;
		default: os << " no flags "; break;
	}
	return os;
}

// according to RFC 2205, addresses must be sorted, so 'push_back' can
// be used. This can be changed in order be "liberal on receive".
void SCOPE_Object::readFromBuffer( INetworkBuffer& buffer, uint16 len ) {
	uint16 count = (len - RSVP_ObjectHeader::size()) / NetAddress::size();
	NetAddress addr;
	for ( ; count > 0; count -= 1 ) {
		buffer >> addr;
		addressList.push_back(addr);
	}
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const SCOPE_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::SCOPE, 1 );
	AddressList::ConstIterator iter = o.addressList.begin();
	for ( ;iter != o.addressList.end(); ++iter ) {
		buffer << *iter;
	}
  return buffer;
}

ostream& operator<< ( ostream& os, const SCOPE_Object& o ) {
	AddressList::ConstIterator iter = o.addressList.begin();
	for ( ;iter != o.addressList.end(); ++iter ) {
		os << " " << *iter;
	}
	return os;
}

INetworkBuffer& operator>> ( INetworkBuffer& buffer, STYLE_Object& o ) {
	buffer >> o.optionVector;
	o.optionVector = o.optionVector % (1<<24);
	return buffer;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const STYLE_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::STYLE, 1 );
	buffer << o.optionVector;
  return buffer;
}

ostream& operator<< ( ostream& os, const STYLE_Object& o ) {
	switch (o.optionVector) {
		case FF: os << "FF";
		break;
		case WF: os << "WF";
		break;
		case SE: os << "SE";
		break;
		default: os << "UNKNOWN";
		break;
	}
	return os;
}

INetworkBuffer& operator>> ( INetworkBuffer& buffer, SENDER_Object& o ) {
	uint16 undefined;
	buffer >> o.srcAddress >>	undefined >> o.srcPort;
	return buffer;
}

ostream& operator<< ( ostream& os, const SENDER_Object& o ) {
  os << o.srcAddress << "/" << (uint32)o.srcPort;
	return os;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const SENDER_TEMPLATE_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::SENDER_TEMPLATE, 7 );
	buffer << o.srcAddress << (uint16)0 << o.srcPort;
  return buffer;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const FILTER_SPEC_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::FILTER_SPEC, 7 );
	buffer << o.srcAddress << (uint16)0 << o.srcPort;
	if ( o.label.getLabel() ) buffer << o.label;
  return buffer;
}

ostream& operator<< ( ostream& os, const FILTER_SPEC_Object& f ) {
	os << (SENDER_Object&)f;
	if (f.label.getLabel()) os << " L:" << f.label;
	return os;
}


const FILTER_SPEC_Object FILTER_SPEC_Object::anyFilter = FILTER_SPEC_Object( NetAddress(0), 0 );

INetworkBuffer& operator>> ( INetworkBuffer& buffer, RESV_CONFIRM_Object& o ) {
	buffer >> o.recvAddress;
	return buffer;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const RESV_CONFIRM_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::RESV_CONFIRM, 1 );
	buffer << o.recvAddress;
  return buffer;
}	

ostream& operator<< ( ostream& os, const RESV_CONFIRM_Object& o ) {
	os << o.recvAddress;
	return os;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const UNKNOWN_Object& o ) {
	buffer << o.header;
	buffer << o.content;
	return buffer;
}

ostream& operator<< ( ostream& os, const UNKNOWN_Object& o ) {
	os << o.header;
	return os;
}

void EXPLICIT_ROUTE_Object::readFromBuffer( INetworkBuffer& buffer, uint16 len ) {
	length = len -= RSVP_ObjectHeader::size();
	AbstractNode a;
	while ( len > 0 ) {
		buffer >> a;
		if ( a.getType() != AbstractNode::Illegal ) abstractNodeList.push_back(a);
		len -= a.getLength();
	}
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const EXPLICIT_ROUTE_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::EXPLICIT_ROUTE, 1 );
	AbstractNodeList::ConstIterator iter = o.abstractNodeList.begin();
	for ( ; iter != o.abstractNodeList.end(); ++iter ) {
		buffer << *iter;
	}
	return buffer;
}

ostream& operator<< ( ostream& os, const EXPLICIT_ROUTE_Object& o ) {
	AbstractNodeList::ConstIterator iter = o.abstractNodeList.begin();
	for ( ; iter != o.abstractNodeList.end(); ++iter ) {
		os << *iter << "--";
	}
	return os;
}

void LABEL_Object::readFromBuffer(INetworkBuffer& buffer, uint16 len, uint8 C_Type)
{
	switch (C_Type){
		case LABEL_MPLS:  
		case LABEL_GENERALIZED: 	
			buffer >> label; labelCType = C_Type; break;
		default: 
			buffer.skip(len); labelCType = 0; return;
	}	
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const LABEL_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::LABEL, o.labelCType);
	buffer << o.label;
  return buffer;
}

ostream& operator<< ( ostream& os, const LABEL_Object& o ) {
	os << (uint32)o.label;
	return os;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const SUGGESTED_LABEL_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::SUGGESTED_LABEL, o.labelCType );
	buffer << o.label;
  return buffer;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const UPSTREAM_LABEL_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::UPSTREAM_LABEL, o.labelCType );
	buffer << o.label;
  return buffer;
}

void LABEL_REQUEST_Object::readFromBuffer(INetworkBuffer& buffer, uint16 len, uint8 C_Type)
{
	switch (C_Type){
		case 1:  buffer >> l3pid; labelType = LABEL_Object::LABEL_MPLS; break;
		case 4:  	buffer >> lspEncodingType >> switchingType >> gPid; labelType = LABEL_Object::LABEL_GENERALIZED; break;
		default: buffer.skip(len); labelType = 0; return;
	}	
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const LABEL_REQUEST_Object& o ) {
	if (o.labelType == LABEL_Object::LABEL_MPLS){
		buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::LABEL_REQUEST, 1 );
		buffer << o.l3pid;
	}
	else{
		buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::LABEL_REQUEST, 4 );
		buffer << o.lspEncodingType << o.switchingType << o.gPid;
	}
  return buffer;
}

ostream& operator<< ( ostream& os, const LABEL_REQUEST_Object& o ) {
	if (o.labelType == LABEL_Object::LABEL_MPLS)
		os << "L3_Pid: " << o.l3pid;
	else
		os << "LSP_Enc: " << (uint32)o.lspEncodingType <<" Sw_Type: "<< (uint32)o.switchingType << " G_Pid: " << (uint32)o.gPid;
	return os;
}

void LABEL_SET_Object::readFromBuffer( INetworkBuffer& buffer, uint16 len ) {
	uint16 dummy;
	buffer >> action >> dummy >> labelType;

	uint16 count = (len - 4 - RSVP_ObjectHeader::size()) / 4;
	uint32 subChannel;
	for ( ; count > 0; count -= 1 ) {
		buffer >> subChannel;
		subChannelList.push_back(subChannel);
	}
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const LABEL_SET_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::LABEL_SET, 1 );
	buffer << o.action << (uint16)0 << o.labelType;
	SortableList<uint32, uint32>::ConstIterator iter = o.subChannelList.begin();
	for ( ;iter != o.subChannelList.end(); ++iter ) {
		buffer << *iter;
	}
  return buffer;
}

ostream& operator<< ( ostream& os, const LABEL_SET_Object& o ) {
	os << "Action: " << (uint32)o.action << "LabelType: " << (uint32)o.labelType << "SubChannels:";
	SortableList<uint32, uint32>::ConstIterator iter = o.subChannelList.begin();
	for ( ;iter != o.subChannelList.end(); ++iter ) {
		os << " " << *iter;
	}
	return os;
}


ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const AbstractNode& a ) {
	buffer << a.typeOrLoose;
	switch (a.getType()) {
		case AbstractNode::IPv4:
			buffer << uint8(8) << NetAddress(a.data.ip4.addr) << a.data.ip4.prefix << uint8(0);
			break;
		case AbstractNode::AS:
			buffer << uint8(4) << a.data.asNum;
			break;
		case AbstractNode::UNumIfID:
			buffer<<uint8(12)<<a.data.uNumIfID.reserved << NetAddress(a.data.uNumIfID.routeID) << a.data.uNumIfID.interfaceID;
			break;
		default:
			FATAL(2)( Log::Fatal, "unrecognized abstract node type:", a.getType() );
			abortProcess();
	}
	return buffer;
}

INetworkBuffer& operator>> ( INetworkBuffer& buffer, AbstractNode& a ) {
	uint8 length, dummy;
	NetAddress addr, rtID;
	
	buffer >> a.typeOrLoose >> length;
	// TODO: check length field
	switch (a.getType()) {
		case AbstractNode::IPv4:
			buffer >> addr >> a.data.ip4.prefix >> length;
			a.data.ip4.addr = addr.rawAddress();
			break;
		case AbstractNode::AS:
			buffer >> a.data.asNum;
			break;
		case AbstractNode::UNumIfID:
			buffer >> a.data.uNumIfID.reserved>> rtID>>a.data.uNumIfID.interfaceID;
			a.data.uNumIfID.routeID = rtID.rawAddress();
			break;
		default:
			ERROR(2)( Log::Error, "skipping unrecognized abstract node type:", a.getType() );
			buffer.skip( dummy - 2 );
			a.typeOrLoose = AbstractNode::Illegal;
			break;
	}
	return buffer;
}

ostream& operator<< ( ostream& os, const AbstractNode& a ) {
	switch (a.getType()) {
		case AbstractNode::IPv4:
			os << "IP4:" << NetAddress(a.data.ip4.addr) << "/" << uint32(a.data.ip4.prefix);
			break;
		case AbstractNode::AS:
			os << "AS:" << a.data.asNum;
			break;
		case AbstractNode::UNumIfID:
			os<<"UNumIfID:"<< "{" << NetAddress(a.data.uNumIfID.routeID) << "," << a.data.uNumIfID.interfaceID << "}";
			break;
		default:
			FATAL(2)( Log::Fatal, "unrecognized abstract node type:", a.getType() );
			abortProcess();
	}
	return os;
}

#if defined(ONEPASS_RESERVATION) 
INetworkBuffer& operator>> ( INetworkBuffer& buffer, DUPLEX_Object& o ) {
	buffer >> o.senderReceivePort >> o.receiverSendPort;
	return buffer;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const DUPLEX_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::DUPLEX, 1 );
	buffer << o.senderReceivePort << o.receiverSendPort;
  return buffer;
}

ostream& operator<< ( ostream& os, const DUPLEX_Object& o ) {
	os << o.senderReceivePort << " " << o.receiverSendPort;
	return os;
}
#endif

#if defined(REFRESH_REDUCTION)
ostream& operator<< ( ostream& os, const MESSAGE_ID_BASE_Object& o ) {
	os << "flags:" << (uint32)o.getFlags() << " epoch:" << o.getEpoch() << " id:" << o.getID();
	return os;
}

INetworkBuffer& operator>> ( INetworkBuffer& buffer, MESSAGE_ID_BASE_Object& o ) {
	buffer >> o.flagsEpoch >> o.id;
	return buffer;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const MESSAGE_ID_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::MESSAGE_ID, 1 );
	buffer << o.flagsEpoch << o.id;
  return buffer;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const MESSAGE_ID_ACK_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::MESSAGE_ID_ACK, 1 );
	buffer << o.flagsEpoch << o.id;
  return buffer;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const class MESSAGE_ID_NACK_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::MESSAGE_ID_ACK, 2 );
	buffer << o.flagsEpoch << o.id;
  return buffer;
}

ostream& operator<< ( ostream& os, const MESSAGE_ID_LIST_Object& o ) {
	os << "flags:" << (uint32)o.getFlags() << " epoch:" << o.getEpoch();
	SimpleList<sint32>::ConstIterator iter = o.idList.begin();
	for ( ; iter != o.idList.end(); ++iter ) {
		os << " id:" << *iter;
	}
	return os;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const MESSAGE_ID_LIST_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::MESSAGE_ID_LIST, 1 );
	buffer << o.flagsEpoch;
	SimpleList<sint32>::ConstIterator iter = o.idList.begin();
	for ( ; iter != o.idList.end(); ++iter ) {
		buffer << *iter;
	}
	return buffer;
}

void MESSAGE_ID_LIST_Object::readFromBuffer( INetworkBuffer& buffer, uint16 len ) {
	buffer >> flagsEpoch;
	uint16 count = (len - (4 + RSVP_ObjectHeader::size())) / sizeof(sint32);
	sint32 id;
	for ( ; count > 0; count -= 1 ) {
		buffer >> id;
		idList.push_back(id);
	}
}

#endif


//////////////////////////////////////////////////////////////////////////
/////        DRAGON UNI Object implementation                         ////
/////////////////////////////////////////////////////////////////////////

void DRAGON_UNI_Object::readFromBuffer(INetworkBuffer& buffer, uint16 len)
{
	buffer >>srcTNA.length >> srcTNA.type >> srcTNA.sub_type >> srcTNA.addr.s_addr >> srcTNA.local_id;
	buffer >>destTNA.length >> destTNA.type >> destTNA.sub_type >> destTNA.addr.s_addr >> destTNA.local_id;
	buffer >>vlanTag.length >> vlanTag.type >> vlanTag.sub_type >> vlanTag.vtag;
	buffer >>ingressChannelName.length >> ingressChannelName.type >> ingressChannelName.sub_type;
	for (uint32 i = 0; i < sizeof(struct CtrlChannel) - 4; i++)
		buffer >> ingressChannelName.name[i];
	buffer >>egressChannelName.length >> egressChannelName.type >> egressChannelName.sub_type;
	for (uint32 i = 0; i < sizeof(struct CtrlChannel) - 4; i++)
		buffer >> egressChannelName.name[i];
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const DRAGON_UNI_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), o.getClassNumber(), 1);
	buffer <<o.srcTNA.length << o.srcTNA.type << o.srcTNA.sub_type << o.srcTNA.addr.s_addr << o.srcTNA.local_id;
	buffer << o.destTNA.length << o.destTNA.type << o.destTNA.sub_type << o.destTNA.addr.s_addr << o.destTNA.local_id;
	buffer << o.vlanTag.length << o.vlanTag.type << o.vlanTag.sub_type << o.vlanTag.vtag;
	buffer << o.ingressChannelName.length << o.ingressChannelName.type << o.ingressChannelName.sub_type;
	for (uint32 i = 0; i < sizeof(struct CtrlChannel) - 4; i++)
		buffer << o.ingressChannelName.name[i];
	buffer << o.egressChannelName.length << o.egressChannelName.type << o.egressChannelName.sub_type;
	for (uint32 i = 0; i < sizeof(struct CtrlChannel) - 4; i++)
		buffer << o.egressChannelName.name[i];
	return buffer;
}

ostream& operator<< ( ostream& os, const DRAGON_UNI_Object& o ) {
	os <<"[ source: " << String( inet_ntoa(o.srcTNA.addr)) <<"/" << o.srcTNA.local_id;
	os <<" <= vtag (" << o.vlanTag.vtag << ") => destination: ";
	os << String( inet_ntoa(o.destTNA.addr)) << "/" << o.destTNA.local_id;
	os << " (via ingressChannel: " << o.ingressChannelName.name << " /egressChannel: " << o.egressChannelName.name;
	os << ") ]";
	return os;
}



//////////////////////////////////////////////////////////////////////////
/////     Generalized UNI Object implementation                       ////
/////////////////////////////////////////////////////////////////////////

void GENERALIZED_UNI_Object::readFromBuffer(INetworkBuffer& buffer, uint16 len)
{
	buffer >>srcTNA.length >> srcTNA.type >> srcTNA.sub_type >> srcTNA.addr.s_addr;
	buffer >>destTNA.length >> destTNA.type >> destTNA.sub_type >> destTNA.addr.s_addr;
	buffer >>egressLabel.length >> egressLabel.type >> egressLabel.sub_type 
		>> *(uint32*)&(egressLabel.u_b0) >> egressLabel.logical_port >> egressLabel.label;
	buffer >>egressLabelUp.length >> egressLabelUp.type >> egressLabelUp.sub_type 
		>> *(uint32*)&(egressLabelUp.u_b0) >> egressLabelUp.logical_port >> egressLabelUp.label;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const GENERALIZED_UNI_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), o.getClassNumber(), 1);
	buffer <<o.srcTNA.length << o.srcTNA.type << o.srcTNA.sub_type << o.srcTNA.addr.s_addr;
	buffer << o.destTNA.length << o.destTNA.type << o.destTNA.sub_type << o.destTNA.addr.s_addr;
	buffer << o.egressLabel.length << o.egressLabel.type << o.egressLabel.sub_type 
		<< *(uint32*)&(o.egressLabel.u_b0) << o.egressLabel.logical_port<< o.egressLabel.label;
	buffer << o.egressLabelUp.length << o.egressLabelUp.type << o.egressLabelUp.sub_type 
		<< *(uint32*)&(o.egressLabelUp.u_b0) << o.egressLabelUp.logical_port << o.egressLabelUp.label;
	return buffer;
}

ostream& operator<< ( ostream& os, const GENERALIZED_UNI_Object& o ) {
	os <<"[G_UNI Source: " << String( inet_ntoa(o.srcTNA.addr) );
	os <<" <=> Destination: " << String( inet_ntoa(o.destTNA.addr) );
	os <<" (Downstream port:"<< o.egressLabel.logical_port << " /Label:" << o.egressLabel.label;
	os <<" (Upstream port:"<< o.egressLabelUp.logical_port << " /Label:" << o.egressLabelUp.label;
	os <<") ]";
	return os;
}

//////////////////////////////////////////////////////////////////////////
/////     DRAGON Extension Inforation Object implementation           ////
/////////////////////////////////////////////////////////////////////////

void DRAGON_EXT_INFO_Object::readFromBuffer(INetworkBuffer& buffer, uint16 len)
{
	//more tlvs --> parse tlv header first?
	int readLength= 0;
	uint16 tlvLength; 
	uint8 tlvType;
	uint8 tlvSubType;
	uint8 tlvChar;
	uint32 i;
	int j;

	len -= 4; // object header
	while (readLength < len)
	{
		buffer >> tlvLength >> tlvType >> tlvSubType;
		switch (tlvType)
		{
		case DRAGON_EXT_SUBOBJ_SERVICE_CONF_ID:
			serviceConfID.length = tlvLength;
			serviceConfID.type = tlvType;
			serviceConfID.sub_type = tlvSubType;
			buffer >> serviceConfID.ucid >> serviceConfID.seqnum;
			SetSubobjFlag(DRAGON_EXT_SUBOBJ_SERVICE_CONF_ID);
			readLength += tlvLength;
			break;
		case DRAGON_EXT_SUBOBJ_EDGE_VLAN_MAPPING:
			edgeVlanMapping.length = tlvLength;
			edgeVlanMapping.type = tlvType;
			edgeVlanMapping.sub_type = tlvSubType;
			buffer >> edgeVlanMapping.ingress_outer_vlantag >> edgeVlanMapping.ingress_inner_vlantag
				>> edgeVlanMapping.trunk_outer_vlantag >> edgeVlanMapping.trunk_inner_vlantag
				>> edgeVlanMapping.egress_outer_vlantag >> edgeVlanMapping.egress_inner_vlantag;
			SetSubobjFlag(DRAGON_EXT_SUBOBJ_EDGE_VLAN_MAPPING);
			readLength += tlvLength;
			break;
		case DRAGON_EXT_SUBOBJ_DTL:
			memset(&DTL, 0, sizeof(DTL_Subobject));
			DTL.length = tlvLength;
			DTL.type = tlvType;
			DTL.sub_type = tlvSubType;
			buffer >> DTL.count;
			assert(DTL.count <= MAX_DTL_LEN);
			for (i = 0; i < DTL.count; i++)
			{
				for (j = 0; j < MAX_DTL_NODENAME_LEN+1; j++)
					buffer >> DTL.hops[i].nodename[j];
				buffer >> DTL.hops[i].linkid;
			}
			SetSubobjFlag(DRAGON_EXT_SUBOBJ_DTL);
			readLength += tlvLength;
			break;
/************** vvv Extension for DRAGON Monitoring vvv *****************/
		case DRAGON_EXT_SUBOBJ_MON_QUERY:
			memset(&monQuery, 0, sizeof(MON_Query_Subobject));
			monQuery.length = tlvLength;
			monQuery.type = tlvType;
			monQuery.sub_type = tlvSubType;
			buffer >> monQuery.ucid >> monQuery.seqnum;
			for (j = 0; j < MAX_MON_NAME_LEN; j++)
				buffer >> monQuery.gri[j];
			SetSubobjFlag(DRAGON_EXT_SUBOBJ_MON_QUERY);
			readLength += tlvLength;
			break;
		case DRAGON_EXT_SUBOBJ_MON_REPLY:
			memset(&monReply, 0, sizeof(MON_Reply_Subobject));
			monReply.length = tlvLength;
			monReply.type = tlvType;
			monReply.sub_type = tlvSubType;
			buffer >> monReply.ucid >> monReply.seqnum;
			for (j = 0; j < MAX_MON_NAME_LEN; j++)
				buffer >> monReply.gri[j];
			buffer >> monReply.switch_info.switch_ip.s_addr >> monReply.switch_info.switch_port 
				>> monReply.switch_info.switch_type >> monReply.switch_info.access_type
				>> monReply.switch_options;
			if ((monReply.switch_options & MON_SWITCH_OPTION_ERROR) != 0 ) { // Error
				; //noop
			}
			if ((monReply.switch_options & MON_SWITCH_OPTION_SUBNET) == 0) { // Ethernet Switch
				buffer >> monReply.circuit_info.vlan_info.vlan_ingress >> monReply.circuit_info.vlan_info.num_ports_ingress;
				for (j = 0; j < MAX_MON_PORT_NUM; j++)
					buffer >> monReply.circuit_info.vlan_info.ports_ingress[j];
				buffer >> monReply.circuit_info.vlan_info.vlan_egress >> monReply.circuit_info.vlan_info.num_ports_egress;
				for (j = 0; j < MAX_MON_PORT_NUM; j++)
					buffer >> monReply.circuit_info.vlan_info.ports_egress[j];
			}
			else { // EoS Subnet
				buffer >> monReply.circuit_info.eos_info[0].subnet_id >> monReply.circuit_info.eos_info[0].first_timeslot 
					>> monReply.circuit_info.eos_info[0].port >> monReply.circuit_info.eos_info[0].ethernet_bw;
				for (j = 0; j < MAX_MON_NAME_LEN; j++)
					buffer >> monReply.circuit_info.eos_info[0].vcg_name[j];
				for (j = 0; j < MAX_MON_NAME_LEN; j++)
					buffer >> monReply.circuit_info.eos_info[0].eflow_in_name[j];
				for (j = 0; j < MAX_MON_NAME_LEN; j++)
					buffer >> monReply.circuit_info.eos_info[0].eflow_out_name[j];
				for (j = 0; j < MAX_MON_NAME_LEN; j++)
					buffer >> monReply.circuit_info.eos_info[0].snc_crs_name[j];
				for (j = 0; j < MAX_MON_NAME_LEN; j++)
					buffer >> monReply.circuit_info.eos_info[0].dtl_name[j];
				if ( (monReply.switch_options & MON_SWITCH_OPTION_SUBNET_SRC) != 0 && (monReply.switch_options & MON_SWITCH_OPTION_SUBNET_DEST) != 0 ) {
					buffer >> monReply.circuit_info.eos_info[1].subnet_id >> monReply.circuit_info.eos_info[1].first_timeslot 
						>> monReply.circuit_info.eos_info[1].port >> monReply.circuit_info.eos_info[1].ethernet_bw;
					for (j = 0; j < MAX_MON_NAME_LEN; j++)
						buffer >> monReply.circuit_info.eos_info[1].vcg_name[j];
					for (j = 0; j < MAX_MON_NAME_LEN; j++)
						buffer >> monReply.circuit_info.eos_info[1].eflow_in_name[j];
					for (j = 0; j < MAX_MON_NAME_LEN; j++)
						buffer >> monReply.circuit_info.eos_info[1].eflow_out_name[j];
					for (j = 0; j < MAX_MON_NAME_LEN; j++)
						buffer >> monReply.circuit_info.eos_info[1].snc_crs_name[j];
					for (j = 0; j < MAX_MON_NAME_LEN; j++)
						buffer >> monReply.circuit_info.eos_info[1].dtl_name[j];
				}
			}
			SetSubobjFlag(DRAGON_EXT_SUBOBJ_MON_REPLY);
			readLength += tlvLength;
			break;
		case DRAGON_EXT_SUBOBJ_MON_NODE_LIST:
			memset(&monNodeList, 0, sizeof(MON_NodeList_Suboject));
			monNodeList.length = tlvLength;
			monNodeList.type = tlvType;
			monNodeList.sub_type = tlvSubType;
			buffer >> monNodeList.count;
			for (i = 0; i < monNodeList.count; i++)
				buffer >> monNodeList.node_list[i].s_addr;
			SetSubobjFlag(DRAGON_EXT_SUBOBJ_MON_NODE_LIST);
			readLength += tlvLength;
			break;
/************** ^^^ Extension for DRAGON Monitoring ^^^ *****************/
		default:
			readLength += tlvLength;
			while( (tlvLength--) > 4 ) buffer >> tlvChar;
			break;
		}
	}
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const DRAGON_EXT_INFO_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::DRAGON_EXT_INFO, 1);
	if (o.HasSubobj(DRAGON_EXT_SUBOBJ_SERVICE_CONF_ID)) {
		buffer <<o.serviceConfID.length << o.serviceConfID.type << o.serviceConfID.sub_type 
			<< o.serviceConfID.ucid << o.serviceConfID.seqnum;
	}
	if (o.HasSubobj(DRAGON_EXT_SUBOBJ_EDGE_VLAN_MAPPING)) {
		buffer << o.edgeVlanMapping.length << o.edgeVlanMapping.type << o.edgeVlanMapping.sub_type
			<<o.edgeVlanMapping.ingress_outer_vlantag << o.edgeVlanMapping.ingress_inner_vlantag 
			<< o.edgeVlanMapping.trunk_outer_vlantag << o.edgeVlanMapping.trunk_inner_vlantag 
			<< o.edgeVlanMapping.egress_outer_vlantag << o.edgeVlanMapping.egress_inner_vlantag;
	}
	if (o.HasSubobj(DRAGON_EXT_SUBOBJ_DTL)) {
		assert(o.DTL.count <= MAX_DTL_LEN);
		buffer << o.DTL.length << o.DTL.type << o.DTL.sub_type<<o.DTL.count;
		uint32 i;
		int j;
		for (i = 0; i < o.DTL.count; i++)
		{
			for (j = 0; j < MAX_DTL_NODENAME_LEN+1; j++)
				buffer << o.DTL.hops[i].nodename[j];
			buffer << o.DTL.hops[i].linkid;
		}
	}

/************** vvv Extension for DRAGON Monitoring vvv *****************/
	if (o.HasSubobj(DRAGON_EXT_SUBOBJ_MON_QUERY)) {
		int i;
		buffer << o.monQuery.length << o.monQuery.type << o.monQuery.sub_type;
		buffer << o.monQuery.ucid << o.monQuery.seqnum;
		for (i = 0; i < MAX_MON_NAME_LEN; i++)
			buffer << o.monQuery.gri[i];
	}

	if (o.HasSubobj(DRAGON_EXT_SUBOBJ_MON_REPLY)) {
		int i;
		buffer << o.monReply.length << o.monReply.type << o.monReply.sub_type;
		buffer << o.monReply.ucid << o.monReply.seqnum;
		for (i = 0; i < MAX_MON_NAME_LEN; i++)
			buffer << o.monReply.gri[i];
		buffer << o.monReply.switch_info.switch_ip.s_addr << o.monReply.switch_info.switch_port 
			<< o.monReply.switch_info.switch_type << o.monReply.switch_info.access_type
			<< o.monReply.switch_options;
		if ((o.monReply.switch_options & MON_SWITCH_OPTION_ERROR) != 0 ) { // Error
			; // noop
		}
		else if ((o.monReply.switch_options & MON_SWITCH_OPTION_SUBNET) == 0) { // Ethernet Switch
			buffer << o.monReply.circuit_info.vlan_info.vlan_ingress << o.monReply.circuit_info.vlan_info.num_ports_ingress;
			for (i = 0; i < MAX_MON_PORT_NUM; i++)
				buffer << o.monReply.circuit_info.vlan_info.ports_ingress[i];
			buffer << o.monReply.circuit_info.vlan_info.vlan_egress << o.monReply.circuit_info.vlan_info.num_ports_egress;
			for (i = 0; i < MAX_MON_PORT_NUM; i++)
				buffer << o.monReply.circuit_info.vlan_info.ports_egress[i];
		}
		else { // EoS Subnet
			buffer << o.monReply.circuit_info.eos_info[0].subnet_id << o.monReply.circuit_info.eos_info[0].first_timeslot 
				<< o.monReply.circuit_info.eos_info[0].port << o.monReply.circuit_info.eos_info[0].ethernet_bw;
			for (i = 0; i < MAX_MON_NAME_LEN; i++)
				buffer << o.monReply.circuit_info.eos_info[0].vcg_name[i];
			for (i = 0; i < MAX_MON_NAME_LEN; i++)
				buffer << o.monReply.circuit_info.eos_info[0].eflow_in_name[i];
			for (i = 0; i < MAX_MON_NAME_LEN; i++)
				buffer << o.monReply.circuit_info.eos_info[0].eflow_out_name[i];
			for (i = 0; i < MAX_MON_NAME_LEN; i++)
				buffer << o.monReply.circuit_info.eos_info[0].snc_crs_name[i];
			for (i = 0; i < MAX_MON_NAME_LEN; i++)
				buffer << o.monReply.circuit_info.eos_info[0].dtl_name[i];
			if ( (o.monReply.switch_options & MON_SWITCH_OPTION_SUBNET_SRC) != 0 && (o.monReply.switch_options & MON_SWITCH_OPTION_SUBNET_DEST) != 0 ) {
				buffer << o.monReply.circuit_info.eos_info[1].subnet_id << o.monReply.circuit_info.eos_info[1].first_timeslot 
					<< o.monReply.circuit_info.eos_info[1].port << o.monReply.circuit_info.eos_info[1].ethernet_bw;
				for (i = 0; i < MAX_MON_NAME_LEN; i++)
					buffer << o.monReply.circuit_info.eos_info[1].vcg_name[i];
				for (i = 0; i < MAX_MON_NAME_LEN; i++)
					buffer << o.monReply.circuit_info.eos_info[1].eflow_in_name[i];
				for (i = 0; i < MAX_MON_NAME_LEN; i++)
					buffer << o.monReply.circuit_info.eos_info[1].eflow_out_name[i];
				for (i = 0; i < MAX_MON_NAME_LEN; i++)
					buffer << o.monReply.circuit_info.eos_info[1].snc_crs_name[i];
				for (i = 0; i < MAX_MON_NAME_LEN; i++)
					buffer << o.monReply.circuit_info.eos_info[1].dtl_name[i];
			}
		}	
	}
	if (o.HasSubobj(DRAGON_EXT_SUBOBJ_MON_NODE_LIST)) {
		buffer << o.monNodeList.length << o.monNodeList.type << o.monNodeList.sub_type << o.monNodeList.count;
		uint32 i;
		for (i = 0; i < o.monNodeList.count; i++)
			buffer << o.monNodeList.node_list[i].s_addr;
	}

/************** ^^^ Extension for DRAGON Monitoring ^^^ *****************/

	return buffer;
}

ostream& operator<< ( ostream& os, const DRAGON_EXT_INFO_Object& o ) {
	os <<"[";
	if (o.HasSubobj(DRAGON_EXT_SUBOBJ_SERVICE_CONF_ID)) {
		os << "(1: Service Confirmation ID: ucid=" << o.serviceConfID.ucid << ", seqnum=" << o.serviceConfID.seqnum << ")";
	}
	if (o.HasSubobj(DRAGON_EXT_SUBOBJ_EDGE_VLAN_MAPPING)) {
		os << " (2: Edge VLAN Tag Mapping: ingress_outer_vtag=" << o.edgeVlanMapping.ingress_outer_vlantag
			<< ", ingress_inner_vtag=" << o.edgeVlanMapping.ingress_inner_vlantag 
			<< ", trunk_outer_vtag=" << o.edgeVlanMapping.trunk_outer_vlantag 
			<< ", trunk_inner_vtag=" << o.edgeVlanMapping.trunk_inner_vlantag 
			<< ", egress_outer_vtag=" << o.edgeVlanMapping.egress_outer_vlantag 
			<< ", egress_inner_vtag=" << o.edgeVlanMapping.egress_inner_vlantag << ")";
	}
	if (o.HasSubobj(DRAGON_EXT_SUBOBJ_DTL)) {
		uint32 i;
		os << "(3: Designated Transport List: ";
		for (i = 0; i < o.DTL.count; i++)
		{
			os <<"-";
			os << (char*)(o.DTL.hops[i].nodename);
			os <<":";
			os << o.DTL.hops[i].linkid;
			os <<"-";
		}
		os << ")";
	}
/************** vvv Extension for DRAGON Monitoring vvv *****************/
	if (o.HasSubobj(DRAGON_EXT_SUBOBJ_MON_QUERY)) {
		os << "(4: MonQuery: ucid=" << o.monQuery.ucid << ", seqnum" << o.monQuery.seqnum;
		os  << ", gri=" << o.monQuery.gri << ")";
	}
	if (o.HasSubobj(DRAGON_EXT_SUBOBJ_MON_REPLY)) {
		os << "(5: MonReply: ucid=" << o.monQuery.ucid << ", seqnum" << o.monQuery.seqnum;
		os  << ", gri=" << o.monReply.gri;
		os << ", switch_ip=" << String( inet_ntoa(o.monReply.switch_info.switch_ip) ) << ", switch_port=" << o.monReply.switch_info.switch_port 
			<< ", switch_type=" << o.monReply.switch_info.switch_type << ", access_type="  <<o.monReply.switch_info.access_type
			<< ", switch_options" << o.monReply.switch_options;
		if ((o.monReply.switch_options & MON_SWITCH_OPTION_ERROR) != 0 ) { // Error
			os << ", query_error_code=" << (o.monReply.switch_options&0xffff);
		}
		else if ((o.monReply.switch_options & MON_SWITCH_OPTION_SUBNET) == 0)
			os << ", ethernet_info";
		else {
			if ((o.monReply.switch_options & MON_SWITCH_OPTION_SUBNET_SRC) != 0)
				os << ", eos_subnet_info_src";
			if ((o.monReply.switch_options & MON_SWITCH_OPTION_SUBNET_DEST) != 0)
				os << ", ero_subnet_info_dest";
		}
		os << ", circuit_data_length=" << (o.monReply.length - MON_REPLY_BASE_SIZE);
		os << ")";
	}
	if (o.HasSubobj(DRAGON_EXT_SUBOBJ_MON_NODE_LIST)) {
		uint32 i;
		os << "(5: MonNodeList: ";
		for (i = 0; i < o.monNodeList.count; i++)
			os << "-" << String( inet_ntoa(o.monNodeList.node_list[i]) ) << "-";
		os << ")";
	}
/************** ^^^ Extension for DRAGON Monitoring ^^^ *****************/
	os <<"]";
	return os;
}

