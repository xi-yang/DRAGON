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
#include "RSVP_Message.h"
#include "RSVP_PacketHeader.h"

Message::~Message() {
	init();
}

void Message::init() {
	if (INTEGRITY_Object_P) {
		INTEGRITY_Object_P->destroy();
		INTEGRITY_Object_P = NULL;
	}
	if (SCOPE_Object_P) {
		SCOPE_Object_P->destroy();
		SCOPE_Object_P = NULL;
	}
	if (ADSPEC_Object_P) {
		ADSPEC_Object_P->destroy();
		ADSPEC_Object_P = NULL;
	}
	PolicyObjectList::ConstIterator pIter = policyList.begin();
	while ( pIter != policyList.end() ) {
		(*pIter)->destroy();
		pIter = policyList.erase( pIter );
	}
	flowDescriptorList.clear();
	UnknownObjectList::Iterator uoIter = unknownObjectList.begin();
	while (uoIter != unknownObjectList.end() ) {
		(*uoIter)->destroy();
		uoIter = unknownObjectList.erase( uoIter );
	}
	if ( EXPLICIT_ROUTE_Object_P ) {
		EXPLICIT_ROUTE_Object_P->destroy();
		EXPLICIT_ROUTE_Object_P = NULL;
	}
	if ( LABEL_SET_Object_P ) {
		LABEL_SET_Object_P->destroy();
		LABEL_SET_Object_P = NULL;
	}
	RSVP_HOP_Object_O = RSVP_HOP_Object();
	
#if defined(REFRESH_REDUCTION)
	MESSAGE_ID_LIST_Object_O.init();
	ackList.clear();
	nackList.clear();
#endif
	length = headerSize();
	objectFlags = 0;
	status = Correct;
	msgType = 0;
}

#define CASE_CREATE(XXX) \
	case RSVP_ObjectHeader:: XXX :\
    if ( m.objectFlags & Message:: XXX ) { \
			ERROR(1)( Log::Error, "ERROR in Message: duplicate " #XXX " object" ); \
    	m.status = Message::Drop; \
    } \
		buffer >> m. XXX ## _Object_O; \
		m.length += m. XXX ## _Object_O.total_size(); \
		m.objectFlags |= Message:: XXX; \
		break;

#define CASE_CREATE2(XXX) \
	case RSVP_ObjectHeader:: XXX:\
		    if ( m.objectFlags & Message:: XXX ) { \
					ERROR(1)( Log::Error, "ERROR in Message: duplicate " #XXX " object" ); \
		    	m.status = Message::Drop; \
		    } \
			m. XXX ## _Object_O.readFromBuffer(buffer, object.getLength(), object.getC_Type());  \
			m.length += m. XXX ## _Object_O.total_size(); \
			m.objectFlags |= Message:: XXX ; \
			break;

INetworkBuffer& operator>> ( INetworkBuffer& buffer, Message& m ) {
                 assert ( buffer.getRemainingSize() >= Message::headerSize() );

	uint16 readLength = 0; // number of bytes copied from message
	uint16 checkSum;
	uint8 reserved;
	buffer.setChecksumStart();
	buffer >> m.versionFlags;
	buffer >> m.msgType;
	buffer >> checkSum;
	buffer >> m.ttl;
	buffer >> reserved;
	buffer >> readLength;
	if ( !buffer.checkCheckSumRSVP( readLength ) ) {
		m.status = Message::Drop;
		// checksum might have been switched during reading it -> switch again
		ERROR(3)( Log::Error, "ERROR in Message: checksum corrupted:", buffer.calculateChecksumRSVP( readLength ), htons(checkSum) );
		return buffer;
	}
	if ( buffer.getRemainingSize() != readLength - Message::headerSize() ) {
		m.status = Message::Drop;
		ERROR(4)( Log::Error, "ERROR in Message: illegal packet, readLength:", readLength, "buffer length:", buffer.getRemainingSize() );
		return buffer;
	}
	RSVP_ObjectHeader object;
	FILTER_SPEC_Object* lastFilter = NULL;
	for (;;) {
		uint16 bufferLength = buffer.getRemainingSize();
		if ( bufferLength == 0 ) {
	break;
		}
		if ( bufferLength < RSVP_ObjectHeader::size() ) {
			m.status = Message::Drop;
			ERROR(3)( Log::Error, "ERROR in Message: illegal packet, remaining buffer:", bufferLength, "-> smaller than object header" );
			return buffer;
		}
		buffer >> object;
		if ( bufferLength < object.getLength() ) {
			m.status = Message::Drop;
			ERROR(4)( Log::Error, "ERROR in Message: illegal packet, remaining buffer:", bufferLength, "object header:", object );
			return buffer;
		}
		
		/*
			//Omitted because Per rfc2205:
			       ...  The BNF implies an order for the objects in a message.  However, in many (but not all) cases,
			       object order makes no logical difference.  An implementation should create messages with the objects in the order shown here,
			       but accept the objects in any permissible order.
			       - An rsvp implementation must be prepared to process an object of form 0x11bbbbbb at any location in an RSVP message.

			if ( object.getClassNum() != RSVP_ObjectHeader::LABEL ){
			if ( ( (m.objectFlags & Message::STYLE) != 0) != (object.getClassNum() == RSVP_ObjectHeader::FLOWSPEC || object.getClassNum() == RSVP_ObjectHeader::FILTER_SPEC) ) {
				ERROR(1)( Log::Error, "ERROR in Message: STYLE and then FLOWSPEC or FILTERSPEC have to be placed at the very end of the message" );
				m.status = Message::Drop;
				return buffer;
			}
		}*/
		// TODO: we currently don't check for the correct position of INTEGRITY
		switch ( object.getClassNum() ) {
		case RSVP_ObjectHeader::null:
			readLength -= RSVP_ObjectHeader::size();
	continue;
		CASE_CREATE(SESSION)
		CASE_CREATE2(SESSION_ATTRIBUTE)
		case RSVP_ObjectHeader::RSVP_HOP:
		    if ( m.objectFlags & Message::RSVP_HOP ) { 
					ERROR(1)( Log::Error, "ERROR in Message: duplicate RSVP_HOP object" );
		    	m.status = Message::Drop;
		    }
			m.RSVP_HOP_Object_O.readFromBuffer(buffer, object.getLength(), object.getC_Type()); 
			m.length += m.RSVP_HOP_Object_O.total_size();
			m.objectFlags |= Message::RSVP_HOP;
			break;
		case RSVP_ObjectHeader::INTEGRITY:
			m.checkINTEGRITY_Object( new INTEGRITY_Object( buffer ) );
			break;
		CASE_CREATE(TIME_VALUES)
		CASE_CREATE(ERROR_SPEC)
		case RSVP_ObjectHeader::SCOPE:
			m.checkSCOPE_Object( new SCOPE_Object( buffer, object.getLength() ) );
			break;
		CASE_CREATE(STYLE)
		case RSVP_ObjectHeader::FLOWSPEC:
			m.checkFLOWSPEC_Object( new FLOWSPEC_Object( buffer , object.getLength(), object.getC_Type()) );
			break;
		case RSVP_ObjectHeader::FILTER_SPEC:
			if ( !m.checkForFilter() ) {
				m.status = Message::Drop;
				ERROR(1)( Log::Error, "ERROR in Message: illegal FILTER_SPEC object" );
			} else {
				lastFilter = &*m.flowDescriptorList.back().filterSpecList.insert_unique( FILTER_SPEC_Object(buffer) );
				m.length += FILTER_SPEC_Object::total_size();
			}
			break;
		CASE_CREATE(SENDER_TEMPLATE)
		CASE_CREATE2(SENDER_TSPEC)
		CASE_CREATE2(SUGGESTED_LABEL)
		CASE_CREATE2(UPSTREAM_LABEL)
		case RSVP_ObjectHeader::ADSPEC:
			m.checkADSPEC_Object( new ADSPEC_Object( buffer ) );
			break;
		case RSVP_ObjectHeader::POLICY_DATA:
			readLength -= m.checkPOLICY_DATA_Object( new POLICY_DATA_Object( buffer, object.getLength() ) );
			break;
		CASE_CREATE(RESV_CONFIRM)
		case RSVP_ObjectHeader::LABEL:
			if ( !lastFilter || lastFilter->hasLabel() ) {
				m.status = Message::Drop;
				ERROR(1)( Log::Error, "ERROR in Message: LABEL without FILTER_SPEC" );
			} else {
				lastFilter->setLabel( buffer, object.getLength(), object.getC_Type());
				m.length += (lastFilter->getLABEL_Object()).total_size();
			}
			break;
		CASE_CREATE2(LABEL_REQUEST)
		case RSVP_ObjectHeader::EXPLICIT_ROUTE:
			m.checkEXPLICIT_ROUTE_Object( new EXPLICIT_ROUTE_Object( buffer, object.getLength() ) );
			break;

		case RSVP_ObjectHeader::LABEL_SET:
			m.checkLABEL_SET_Object( new LABEL_SET_Object( buffer, object.getLength() ) );
			break;
#if defined(ONEPASS_RESERVATION) 
		CASE_CREATE(DUPLEX)
#endif
#if defined(REFRESH_REDUCTION)
		CASE_CREATE(MESSAGE_ID)
		case RSVP_ObjectHeader::MESSAGE_ID_ACK:
		switch( object.getC_Type() ) {
			case 1:
				m.addMESSAGE_ID_ACK_Object( MESSAGE_ID_ACK_Object( buffer ) );
				break;
			case 2:
				m.addMESSAGE_ID_NACK_Object( MESSAGE_ID_NACK_Object( buffer ) );
				break;
			default:
				assert(0);
		}
		break;
		case RSVP_ObjectHeader::MESSAGE_ID_LIST:
			if ( m.objectFlags & Message::MESSAGE_ID_LIST ) {
				ERROR(1)( Log::Error, "ERROR in Message: duplicate MESSAGE_ID_LIST object" );
  	  	m.status = Message::Drop;
			}
			m.MESSAGE_ID_LIST_Object_O.readFromBuffer( buffer, object.getLength() );
			m.objectFlags |= Message::MESSAGE_ID_LIST;
			m.length += m.MESSAGE_ID_LIST_Object_O.total_size();
		break;
#endif
		default:
			LOG(2)( Log::Msg, "cannot determine RSVP object with class num:", (uint16)object.getClassNum() );
			if ( object.getClassNum() & 0x80 ) {
				if ( object.getClassNum() & 0x40 ) {
					UNKNOWN_Object *uo = new UNKNOWN_Object( object, buffer );
					m.unknownObjectList.push_back( uo );
					m.length += object.getLength();
				} else {
					readLength -= object.getLength();
				}
			} else {
				readLength -= object.getLength();
				/*if ( m.status == Message::Correct && (m.msgType == Message::Path || m.msgType == Message::Resv || m.msgType == Message::PathResv) ) {
					m.status = Message::Reject;
				}*/
			}
			buffer.skip( object.getLength() - RSVP_ObjectHeader::size() );
		} // switch
	} // for
	//@@@@ A temp solution for message reading error would be removing the below  if clause.
	if ( readLength != m.length ) {
		m.status = Message::Drop;
		ERROR(4)( Log::Error, "ERROR in Message: message length field was set incorrect:", readLength, "!=", m.length );
		return buffer;
	}
	if ( m.msgType == Message::ResvTear && m.STYLE_Object_O.getStyle() == WF && m.flowDescriptorList.empty() ) {
		m.flowDescriptorList.push_back( new FLOWSPEC_Object );
	}
	m.checkStatus();
	return buffer;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const Message& m ) {
#if defined(RSVP_CHECKS)
	m.checkStatus();
#endif
	if ( m.getStatus() != Message::Correct ) {
		FATAL(1)( Log::Fatal, "FATAL INTERNAL ERROR: tried to send illegal RSVP message" );
		abortProcess();
	}
	buffer.setChecksumStart();
	buffer << m.versionFlags;
	buffer << m.msgType;
	buffer << (uint16)0;
	buffer << m.ttl;
	buffer << (uint8)0;
	buffer << m.length;
	if (m.INTEGRITY_Object_P) buffer << *m.INTEGRITY_Object_P;
#if defined(REFRESH_REDUCTION)
	if (m.objectFlags & Message::MESSAGE_ID) buffer << m.MESSAGE_ID_Object_O;
	MESSAGE_ID_ACK_List::ConstIterator ackIter = m.ackList.begin();
	for ( ; ackIter != m.ackList.end(); ++ackIter ) {
		buffer << *ackIter;
	}
	MESSAGE_ID_NACK_List::ConstIterator nackIter = m.nackList.begin();
	for ( ; nackIter != m.nackList.end(); ++nackIter ) {
		buffer << *nackIter;
	}
	if (m.objectFlags & Message::MESSAGE_ID_LIST) buffer << m.MESSAGE_ID_LIST_Object_O;
#endif
	if (m.objectFlags & Message::SESSION) buffer << m.SESSION_Object_O;
	if (m.objectFlags & Message::RSVP_HOP) buffer << m.RSVP_HOP_Object_O;
	if (m.objectFlags & Message::TIME_VALUES) buffer << m.TIME_VALUES_Object_O;
	if (m.objectFlags & Message::LABEL_REQUEST) buffer << m.LABEL_REQUEST_Object_O;
	if (m.objectFlags & Message::SUGGESTED_LABEL) buffer << m.SUGGESTED_LABEL_Object_O;
	if (m.objectFlags & Message::UPSTREAM_LABEL) buffer << m.UPSTREAM_LABEL_Object_O;
	if (m.EXPLICIT_ROUTE_Object_P) buffer << *m.EXPLICIT_ROUTE_Object_P;
	if (m.LABEL_SET_Object_P) buffer << *m.LABEL_SET_Object_P;
	if (m.objectFlags & Message::SESSION_ATTRIBUTE) buffer << m.SESSION_ATTRIBUTE_Object_O;
	if (m.objectFlags & Message::ERROR_SPEC) buffer << m.ERROR_SPEC_Object_O;
	if (m.objectFlags & Message::RESV_CONFIRM) buffer << m.RESV_CONFIRM_Object_O;
	if (m.SCOPE_Object_P) buffer << *m.SCOPE_Object_P;
	PolicyObjectList::ConstIterator pIter = m.policyList.begin();
	for ( ; pIter != m.policyList.end(); ++pIter ) {
		buffer << **pIter;
	}
	if (m.objectFlags & Message::SENDER_TEMPLATE) buffer << m.SENDER_TEMPLATE_Object_O;
	if (m.objectFlags & Message::SENDER_TSPEC) buffer << m.SENDER_TSPEC_Object_O;
	if (m.ADSPEC_Object_P) buffer << *m.ADSPEC_Object_P;
	UnknownObjectList::ConstIterator uoIter = m.unknownObjectList.begin();
	for ( ; uoIter != m.unknownObjectList.end(); ++uoIter ) {
		buffer << **uoIter;
	}
#if defined(ONEPASS_RESERVATION) 
	if (m.objectFlags & Message::DUPLEX) buffer << m.DUPLEX_Object_O;
#endif
	if (m.objectFlags & Message::STYLE) {
		buffer << m.STYLE_Object_O;
		FlowDescriptorList::ConstIterator fdIter = m.flowDescriptorList.begin();
		for ( ; fdIter != m.flowDescriptorList.end(); ++fdIter ) {
			buffer << *fdIter;
		}
	}
	buffer.setChecksumRSVP( m.length );
	return buffer;
}

#define CHECK_OBJECT_REF(XXX) \
	if ( XXX ## _Object_P ) { \
		o->destroy(); \
		status = Drop; \
		ERROR(1)( Log::Error, "ERROR in Message: duplicate " #XXX " object" ); \
	} \
	XXX ## _Object_P = o; \
	length += o->total_size();

void Message::checkINTEGRITY_Object( const INTEGRITY_Object* o ) {
	CHECK_OBJECT_REF(INTEGRITY)
}
void Message::checkSCOPE_Object( const SCOPE_Object* o ) {
	CHECK_OBJECT_REF(SCOPE)
}
void Message::checkFLOWSPEC_Object( const FLOWSPEC_Object* o ) {
	if ( !flowDescriptorList.empty() ) {
		if ( flowDescriptorList.back().filterSpecList.empty() ) {
			o->destroy();
			status = Drop;
			ERROR(1)( Log::Error, "ERROR in Message: two consecutive FLOWSPEC objects" );
			return;
		}
	}
	if ( o->getServiceNumber() == 0 ) {
		o->destroy();
		status = Drop;
		ERROR(1)( Log::Error, "ERROR in Message: service number 0 -> illegal FLOWSPEC object" );
		return;
	}
	flowDescriptorList.push_back( o );
	length += o->total_size();
}

void Message::checkEXPLICIT_ROUTE_Object( const EXPLICIT_ROUTE_Object* o ) {
	CHECK_OBJECT_REF(EXPLICIT_ROUTE)
}

void Message::checkLABEL_SET_Object( const LABEL_SET_Object* o ) {
	CHECK_OBJECT_REF(LABEL_SET)
}


bool Message::checkForFilter() {
	if ( flowDescriptorList.empty() && msgType == ResvTear ) {
		flowDescriptorList.push_back( NULL );
	}
	switch ( STYLE_Object_O.getStyle() ) {
	case WF:
		return false;
	case FF:
		if ( flowDescriptorList.empty() ) return false;
		if ( !flowDescriptorList.back().filterSpecList.empty() ) {
			const FLOWSPEC_Object* fs = flowDescriptorList.back().getFlowspec();
			if ( msgType == ResvTear ) {
				flowDescriptorList.push_back( NULL );
			} else if ( fs ) {
				flowDescriptorList.push_back( fs->borrow() );
				length += fs->total_size();
			} else {
				return false;
			}
		}
		return true;
	case SE:
		return true;
	default:
		return false;
	}
}

void Message::checkADSPEC_Object( const ADSPEC_Object* o ) {
	CHECK_OBJECT_REF(ADSPEC)
}
uint16 Message::checkPOLICY_DATA_Object( const POLICY_DATA_Object* o ) {
	if ( o->checkCorrectness() ) {
		policyList.push_back( o );
		length += o->total_size();
		return 0;
	} else {
		return o->total_size();
	}
}
void Message::addFILTER_SPEC_Objects( const FilterSpecList& o ) {
	if ( !o.empty() && !checkForFilter() ) {
		status = Drop;
		ERROR(1)( Log::Error, "ERROR in Message: illegal FILTER_SPEC object" );
		return;
	}
	FilterSpecList& filterSpecList = flowDescriptorList.back().filterSpecList;
	filterSpecList.insert( filterSpecList.end(), o.begin(), o.end() );
	length += o.size() * FILTER_SPEC_Object::total_size();
	FilterSpecList::ConstIterator fiter = o.begin();
	for ( ; fiter != o.end(); ++fiter ) {
		if ( (*fiter).hasLabel() ) length += (*fiter).getLABEL_Object().total_size();
	}
}

void Message::addUnknownObjects( const UnknownObjectList& uoList ) {
	UnknownObjectList::ConstIterator uoIter = uoList.begin();
	for ( ; uoIter != uoList.end(); ++uoIter ) {
		unknownObjectList.push_back( (*uoIter)->borrow() );
		length += (*uoIter)->total_size();
	}
}

void Message::setFLOWSPEC_Object( const FLOWSPEC_Object& o ) {
	if ( o.getServiceNumber() == 0 ) {
		status = Drop;
		ERROR(1)( Log::Error, "ERROR in Message: service number 0 -> illegal FLOWSPEC object" );
		return;
	}
	if ( flowDescriptorList.empty() ) {
		flowDescriptorList.push_back( o.borrow() );
		length += o.total_size();
	} else {
		flowDescriptorList.back().setFlowspec( &o );
	}
}

void Message::revertToError( const ERROR_SPEC_Object& error ) {
	switch( msgType ) {
	case Path:
	case PathResv:
		msgType = PathErr;
		clearRSVP_HOP_Object(getRSVP_HOP_Object());
		clearEXPLICIT_ROUTE_Object();
		break;
	case Resv:
		msgType = ResvErr;
		if ( objectFlags & RESV_CONFIRM ) {
			clearRESV_CONFIRM_Object();
		}
		RSVP_HOP_Object_O.setAddress( error.getAddress() );
		break;
	default:
		FATAL(1)( Log::Fatal, "FATAL INTERNAL ERROR: revertToError only for PATH and RESV. aborting..." );
		abortProcess();
	}
	status = Correct;
#if defined(REFRESH_REDUCTION)
	if ( objectFlags & MESSAGE_ID ) {
		objectFlags &= ~MESSAGE_ID;
		length -= MESSAGE_ID_Object::total_size();
	}
#endif
	objectFlags &= ~TIME_VALUES;
	length -= TIME_VALUES_Object::total_size();
	setERROR_SPEC_Object( error );
	ttl = 63;
}

#if defined(ONEPASS_RESERVATION) 
void Message::switchDuplex( const NetAddress& addr ) {
                                                 assert( msgType == PathResv );
	NetAddress tmp = SESSION_Object_O.getDestAddress();
	SESSION_Object_O.setDestAddress( SENDER_TEMPLATE_Object_O.getSrcAddress() );
	SESSION_Object_O.setTunnelId( DUPLEX_Object_O.getSenderReceivePort() );
	SENDER_TEMPLATE_Object_O.setSrcAddress( tmp );
	SENDER_TEMPLATE_Object_O.setLspId( DUPLEX_Object_O.getReceiverSendPort() );
	clearRSVP_HOP_Object(getRSVP_HOP_Object());
	RSVP_HOP_TLV_SUB_Object tlv;
	setRSVP_HOP_Object( RSVP_HOP_Object( addr, 0, tlv) );
	clearDUPLEX_Object();
}
#endif

ostream& operator<< ( ostream& os, const Message& m ) {
	switch (m.msgType) {
		case Message::InitAPI:
                                      assert(m.objectFlags & Message::SESSION);
		os << "InitAPI";
		break;
		case Message::RemoveAPI:
                                      assert(m.objectFlags & Message::SESSION);
		os << "RemoveAPI";
		break;
		case Message::Path: os << "PATH"; break;
		case Message::Resv: os << "RESV"; break;
		case Message::PathErr: os << "PERR"; break;
		case Message::ResvErr: os << "RERR"; break;
		case Message::PathTear: os << "PTEAR"; break;
		case Message::ResvTear: os << "RTEAR"; break;
		case Message::ResvConf: os << "RCONF"; break;
#if defined(ONEPASS_RESERVATION)
		case Message::PathResv: os << "PATHRESV"; break;
#endif
#if defined(REFRESH_REDUCTION)
		case Message::Ack: os << "ACK"; break;
		case Message::Srefresh: os << "SREFRESH"; break;
#endif
		default: os << "UNKNOWN"; break;
	}
	os << " " << (uint32)m.getVersion() << " " << (uint32)m.getFlags()
		<< " ttl:" << (uint32)m.ttl << " length:" << (uint32)m.length;
	if (m.INTEGRITY_Object_P) os << endl << " INTEGRITY:" << *m.INTEGRITY_Object_P;
#if defined(REFRESH_REDUCTION)
	if (m.objectFlags & Message::MESSAGE_ID) os << endl << " MESSAGE_ID:" << m.MESSAGE_ID_Object_O;
	MESSAGE_ID_ACK_List::ConstIterator ackIter = m.ackList.begin();
	for ( ; ackIter != m.ackList.end(); ++ackIter ) {
		os << endl << " MESSAGE_ID_ACK:" << *ackIter;
	}
	MESSAGE_ID_NACK_List::ConstIterator nackIter = m.nackList.begin();
	for ( ; nackIter != m.nackList.end(); ++nackIter ) {
		os << endl << " MESSAGE_ID_NACK:" << *nackIter;
	}
	if (m.objectFlags & Message::MESSAGE_ID_LIST) os << endl << " MESSAGE_ID_LIST:" << m.MESSAGE_ID_LIST_Object_O;
#endif
	if (m.objectFlags & Message::SESSION) os << endl << " SESSION:" << m.SESSION_Object_O;
	if (m.objectFlags & Message::RSVP_HOP) os << endl << " RSVP_HOP:" << m.RSVP_HOP_Object_O;
	if (m.objectFlags & Message::TIME_VALUES) os << endl << " TIME_VALUES:" << m.TIME_VALUES_Object_O;
	if (m.objectFlags & Message::ERROR_SPEC) os << endl << " ERROR_SPEC:" << m.ERROR_SPEC_Object_O;
	if (m.objectFlags & Message::RESV_CONFIRM) os << endl << " RESV_CONFIRM:" << m.RESV_CONFIRM_Object_O;
	if (m.SCOPE_Object_P) os << endl << " SCOPE:" << *m.SCOPE_Object_P;
	PolicyObjectList::ConstIterator pIter = m.policyList.begin();
	for ( ; pIter != m.policyList.end(); ++pIter ) {
		os << endl << " POLICY_DATA:" << **pIter;
	}
	if (m.objectFlags & Message::SENDER_TEMPLATE) os << endl << " SENDER_TEMPLATE:" << m.SENDER_TEMPLATE_Object_O;
	if (m.objectFlags & Message::SENDER_TSPEC) os << endl << " SENDER_TSPEC:" << m.SENDER_TSPEC_Object_O;
	if (m.ADSPEC_Object_P) os << endl << " ADSPEC:" << *m.ADSPEC_Object_P;
#if defined(ONEPASS_RESERVATION) 
	if (m.objectFlags & Message::DUPLEX) os << endl << " DUPLEX:" << m.DUPLEX_Object_O;
#endif
	if (m.objectFlags & Message::STYLE) {
		os << endl << " STYLE:" << m.STYLE_Object_O;
		FlowDescriptorList::ConstIterator fdIter = m.flowDescriptorList.begin();
		for ( ; fdIter != m.flowDescriptorList.end(); ++fdIter ) {
			os << endl << " FDESC:" << *fdIter;
		}
	}
	if (m.objectFlags & Message::LABEL_REQUEST) os << endl << " LABEL_REQUEST:" << m.LABEL_REQUEST_Object_O;
	if (m.EXPLICIT_ROUTE_Object_P) os << endl << " EXPLICIT_ROUTE:" << *m.EXPLICIT_ROUTE_Object_P;
	if (m.LABEL_SET_Object_P) os << endl << "LABEL_SET: " << *m.LABEL_SET_Object_P;
	if (m.objectFlags & Message::SUGGESTED_LABEL) os << endl << "SUGGESTED_LABEL: " << m.SUGGESTED_LABEL_Object_O;
	if (m.objectFlags & Message::UPSTREAM_LABEL) os << endl << "UPSTREAM_LABEL: " << m.UPSTREAM_LABEL_Object_O;
	if (m.objectFlags & Message::SESSION_ATTRIBUTE) os << endl << "SESSION_ATTRIBUTE: " << m.SESSION_ATTRIBUTE_Object_O;
	UnknownObjectList::ConstIterator uoIter = m.unknownObjectList.begin();
	for ( ; uoIter != m.unknownObjectList.end(); ++uoIter ) {
		os << endl << " UNKNOWN:" << **uoIter;
	}
	return os;
}

#define REQUIRED(XXX)\
	if ( !(objectFlags & XXX) ) { ERROR(1)( Log::Error, "ERROR in Message: missing " #XXX " object" ); status = Drop; return; }

#define EXCLUDED(XXX)\
	if ( objectFlags & XXX ) { ERROR(1)( Log::Error, "ERROR in Message: unexpected " #XXX " object" ); status = Drop; return; }

#define EXCLUDED_P(XXX)\
	if ( XXX ## _Object_P ) { ERROR(1)( Log::Error, "ERROR in Message: unexpected " #XXX " object" ); status = Drop; return; }

void Message::checkStatus() const {
	switch( msgType ) {
	case Path:
		REQUIRED( SESSION )
		REQUIRED( RSVP_HOP )
		REQUIRED( TIME_VALUES )
		REQUIRED( SENDER_TEMPLATE )
		REQUIRED( SENDER_TSPEC )
		REQUIRED( LABEL_REQUEST )
		EXCLUDED( ERROR_SPEC )
		EXCLUDED_P( SCOPE )
		EXCLUDED( STYLE )
		EXCLUDED( RESV_CONFIRM )
#if defined(ONEPASS_RESERVATION)
		EXCLUDED( DUPLEX )
#endif
#if defined(REFRESH_REDUCTION)
		EXCLUDED( MESSAGE_ID_LIST )
#endif
		if ( !flowDescriptorList.empty() ) {
			ERROR(1)( Log::Error, "ERROR in Message:  flow descriptor in PATH message" );
			status = Drop;
		}
		break;
#if defined(ONEPASS_RESERVATION)
	case PathResv:
		REQUIRED( SESSION )
		REQUIRED( RSVP_HOP )
		REQUIRED( TIME_VALUES )
		REQUIRED( SENDER_TEMPLATE )
		REQUIRED( SENDER_TSPEC )
		EXCLUDED( ERROR_SPEC )
		EXCLUDED_P( SCOPE )
		EXCLUDED( STYLE )
		EXCLUDED( RESV_CONFIRM )
		EXCLUDED( LABEL_REQUEST )
		EXCLUDED_P( EXPLICIT_ROUTE )
		EXCLUDED_P( LABEL_SET )
#if defined(REFRESH_REDUCTION)
		EXCLUDED( MESSAGE_ID_LIST )
#endif
		if ( !flowDescriptorList.empty() ) {
			ERROR(1)( Log::Error, "ERROR in Message:  flow descriptor in PATH message" );
			status = Drop;
		}
		break;
#endif
	case Resv:
		REQUIRED( SESSION )
		REQUIRED( RSVP_HOP )
		REQUIRED( TIME_VALUES )
		REQUIRED( STYLE )
		EXCLUDED( ERROR_SPEC )
		EXCLUDED( SENDER_TEMPLATE )
		EXCLUDED( SENDER_TSPEC )
		EXCLUDED_P( ADSPEC )
		EXCLUDED( LABEL_REQUEST )
		EXCLUDED_P( EXPLICIT_ROUTE )
		EXCLUDED_P(LABEL_SET)
#if defined(ONEPASS_RESERVATION)
		EXCLUDED( DUPLEX )
#endif
#if defined(REFRESH_REDUCTION)
		EXCLUDED( MESSAGE_ID_LIST )
#endif
		if ( !checkFlowdescList() ) {
			status = Drop;
		}
		break;
	case PathErr:
		REQUIRED( SESSION )
		REQUIRED( ERROR_SPEC )
		EXCLUDED( RSVP_HOP )
		EXCLUDED( TIME_VALUES )
		EXCLUDED_P( SCOPE )
		EXCLUDED( STYLE )
		EXCLUDED( RESV_CONFIRM )
#if defined(REFRESH_REDUCTION)
		EXCLUDED( MESSAGE_ID_LIST )
#endif
		if ( !flowDescriptorList.empty() ) {
			ERROR(1)( Log::Error, "ERROR in Message: flow descriptor in PERR message" );
			status = Drop;
		}
		break;
	case ResvErr:
		REQUIRED( SESSION ) 
		REQUIRED( RSVP_HOP )
		REQUIRED( ERROR_SPEC )
		REQUIRED( STYLE )
		EXCLUDED( TIME_VALUES )
		EXCLUDED( SENDER_TEMPLATE )
		EXCLUDED( SENDER_TSPEC )
		EXCLUDED_P( ADSPEC )
		EXCLUDED( RESV_CONFIRM )
		EXCLUDED( LABEL_REQUEST )
		EXCLUDED_P( EXPLICIT_ROUTE )
		EXCLUDED_P(LABEL_SET)
#if defined(ONEPASS_RESERVATION)
		EXCLUDED( DUPLEX )
#endif
#if defined(REFRESH_REDUCTION)
		EXCLUDED( MESSAGE_ID_LIST )
#endif
		if ( !checkFlowdescList() ) {
			status = Drop;
		}
		break;
	case PathTear:
		REQUIRED( SESSION )
		REQUIRED( RSVP_HOP )
		REQUIRED( SENDER_TEMPLATE )
		EXCLUDED( TIME_VALUES )
		EXCLUDED( ERROR_SPEC )
		EXCLUDED_P( SCOPE )
		EXCLUDED( STYLE )
		EXCLUDED( RESV_CONFIRM )
		EXCLUDED( LABEL_REQUEST )
		EXCLUDED_P( EXPLICIT_ROUTE )
		EXCLUDED_P(LABEL_SET)
#if defined(REFRESH_REDUCTION)
		EXCLUDED( MESSAGE_ID_LIST )
#endif
		if ( !flowDescriptorList.empty() ) {
			ERROR(1)( Log::Error, "ERROR in Message: flow descriptor in PTEAR message" );
			status = Drop;
		}
		break;
	case ResvTear:
		REQUIRED( SESSION )
		REQUIRED( RSVP_HOP )
		REQUIRED( STYLE )
		EXCLUDED( TIME_VALUES )
		EXCLUDED( ERROR_SPEC )
		EXCLUDED( SENDER_TEMPLATE )
		EXCLUDED( SENDER_TSPEC )
		EXCLUDED_P( ADSPEC )
		EXCLUDED( RESV_CONFIRM )
		EXCLUDED( LABEL_REQUEST )
		EXCLUDED_P( EXPLICIT_ROUTE )
		EXCLUDED_P(LABEL_SET)
#if defined(ONEPASS_RESERVATION)
		EXCLUDED( DUPLEX )
#endif
#if defined(REFRESH_REDUCTION)
		EXCLUDED( MESSAGE_ID_LIST )
#endif
		if ( !checkFlowdescList() ) {
			status = Drop;
		}
		break;
	case ResvConf:
		REQUIRED( SESSION )
		REQUIRED( ERROR_SPEC )
		REQUIRED( RESV_CONFIRM )
		REQUIRED( STYLE )
		REQUIRED( ERROR_SPEC )
		EXCLUDED( RSVP_HOP )
		EXCLUDED( TIME_VALUES )
		EXCLUDED_P( SCOPE )
		EXCLUDED( SENDER_TEMPLATE )
		EXCLUDED( SENDER_TSPEC )
		EXCLUDED_P( ADSPEC )
		EXCLUDED( LABEL_REQUEST )
		EXCLUDED_P( EXPLICIT_ROUTE )
		EXCLUDED_P(LABEL_SET)
#if defined(ONEPASS_RESERVATION)
		EXCLUDED( DUPLEX )
#endif
#if defined(REFRESH_REDUCTION)
		EXCLUDED( MESSAGE_ID_LIST )
#endif
		if ( !checkFlowdescList() ) {
			status = Drop;
		}
		break;
	case InitAPI:
	case RemoveAPI:
		REQUIRED( SESSION )
		break;
#if defined(REFRESH_REDUCTION)
	case Message::Srefresh:
		REQUIRED( MESSAGE_ID_LIST )
	case Message::Ack:
		break;
#endif
	//@@@@hacked
	case Message::AddLocalId:
	case Message::DeleteLocalId:
		break;
	default:
		ERROR(2)( Log::Error, "ERROR in Message: unknown message type: ", (uint16)msgType );
		status = Drop;
	}
	PolicyObjectList::ConstIterator pIter = policyList.begin();
	for ( ; pIter != policyList.end(); ++pIter ) {
		if ( !(*pIter)->checkCorrectness() ) {
			ERROR(2)( Log::Error, "ERROR in Message: error in POLICY_DATA_Object", **pIter );
				status = Drop;
	break;
		}
	}
}

bool Message::checkFlowdescList() const {
	// TODO: these tests are partially redundant with other checks above
	// in particular: checkForFilter() and set/checkFLOWSPEC_Object
	// however, leave the other ones in for early error detection
	switch ( STYLE_Object_O.getStyle() ) {
	case FF: {
		if ( flowDescriptorList.empty() ) {
			ERROR(1)( Log::Error, "ERROR in Message: no flow descriptor in FF R-type message" );
			return false;
		}
		FlowDescriptorList::ConstIterator fdIter = flowDescriptorList.begin();
		for ( ; fdIter != flowDescriptorList.end(); ++fdIter ) {
			if ( (*fdIter).filterSpecList.size() != 1 ) {
				ERROR(1)( Log::Error, "ERROR in Message: not exactly one filter spec in FF R-type message" );
				return false;
			}
		}
	}	break;
	case WF:
		if ( flowDescriptorList.size() != 1 && msgType != ResvTear ) {
			ERROR(1)( Log::Error, "ERROR in Message: not exactly one flow descriptor in WF RESV/RERR message" );
			return false;
		}
		if ( !flowDescriptorList.front().filterSpecList.empty() ) {
			ERROR(1)( Log::Error, "ERROR in Message: filter spec in WF R-type message" );
			return false;
		}
		break;
	case SE:
		if ( flowDescriptorList.size() != 1 ) {
			ERROR(1)( Log::Error, "ERROR in Message: not exactly one flow descriptor in SE RXXX message" );
			return false;
		}
		if ( flowDescriptorList.front().filterSpecList.empty() ) {
			ERROR(1)( Log::Error, "ERROR in Message: no filter spec in SE RXXX message" );
			return false;
		}
		break;
	default:
		ERROR(2)( Log::Error, "ERROR in Message: unkown filter style", (uint32)STYLE_Object_O.getStyle() );
		return false;
		break;
	}
	return true;
}

#if defined(LOG_ON)
ostream& operator<< ( ostream& os, const MessagePrintShort& m ) {
	switch (m.getMsgType()) {
		case Message::InitAPI:  os << "InitAPI"; break;
		case Message::RemoveAPI:  os << "RemoveAPI"; break;
		case Message::Path: os << "PATH"; break;
		case Message::Resv: os << "RESV"; break;
		case Message::PathErr: os << "PERR"; break;
		case Message::ResvErr: os << "RERR"; break;
		case Message::PathTear: os << "PTEAR"; break;
		case Message::ResvTear: os << "RTEAR"; break;
		case Message::ResvConf: os << "RCONF"; break;
#if defined(ONEPASS_RESERVATION)
		case Message::PathResv: os << "PATHRESV"; break;
#endif
#if defined(REFRESH_REDUCTION)
		case Message::Ack: os << "ACK"; return os;
		case Message::Srefresh: os << "SREFRESH"; return os;
#endif
		default: os << "UNKNOWN"; break;
	}
	os << " for " << m.getSESSION_Object().getDestAddress()
		<< "/" << (uint16)m.getSESSION_Object().getTunnelId()
		<< "," << (uint32)m.getSESSION_Object().getExtendedTunnelId();
	return os;
}
#endif
