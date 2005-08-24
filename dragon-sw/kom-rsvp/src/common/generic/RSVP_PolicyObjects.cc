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
#include "RSVP_PolicyObjects.h"
#include "RSVP_PolicyOptions.h"
#include "RSVP_ChargingElements.h"
#include "RSVP_ProtocolObjects.h"

void POLICY_DATA_Object::destructor() {
	UCPEList::Iterator iter = ucpeList.begin();
	for (;iter != ucpeList.end(); ++iter) {
		delete *iter;
	}
	if (filterSpec) delete filterSpec;
	if (scope) scope->destroy();
	if (integrity) integrity->destroy();
	if (policyRefreshPeriod) delete policyRefreshPeriod;
}

void POLICY_DATA_Object::setFILTER_SPEC_Object( const FILTER_SPEC_Object *object ) {
	if (filterSpec) {
		delete object;
		correct = false;
		ERROR(1)( Log::Error, "ERROR: POLICY_DATA_Object: duplicate FILTER_SPEC_Object" );
		return;
	}
	filterSpec = object;
	length += object->total_size();
	dataOffset += object->total_size();
}

void POLICY_DATA_Object::setSCOPE_Object( const SCOPE_Object  *object ) {
	if (scope) {
		object->destroy();
		correct = false;
		ERROR(1)( Log::Error, "ERROR: POLICY_DATA_Object: duplicate SCOPE_Object" );
		return;
	}
	scope = object;
	length += object->total_size();
	dataOffset += object->total_size();
}

void POLICY_DATA_Object::setINTEGRITY_Object( const INTEGRITY_Object *object ) {
	if (integrity) {
		object->destroy();
		correct = false;
		ERROR(1)( Log::Error, "ERROR: POLICY_DATA_Object: duplicate INTEGRITY_Object" );
		return;
	}
	integrity = object;
	length += object->total_size();
	dataOffset += object->total_size();
}

void POLICY_DATA_Object::setPolicyRefreshPeriod( const PolicyRefreshPeriod *object ) {
	if (policyRefreshPeriod) {
		delete object;
		correct = false;
		ERROR(1)( Log::Error, "ERROR: POLICY_DATA_Object: duplicate PolicyRefreshPeriod" );
		return;
	} 
	policyRefreshPeriod = object;
	length += object->total_size();
	dataOffset += object->total_size();
}

void POLICY_DATA_Object::addUCPE( const UCPE* object ) {
	ucpeList.push_back(object);
	length += (object->size() + PolicyElement::size());
}
 
INetworkBuffer& operator>> ( INetworkBuffer& buffer, POLICY_DATA_Object& o ) { 
	buffer >> o.length;
	buffer >> o.dataOffset;
	buffer >> o.defaultGateway;
	uint32 policyDataObjectLength = o.length; // end of whole POLICY_DATA_Object
	o.length = NetAddress::size()+4;
	uint16 pDataOffset = o.dataOffset - RSVP_ObjectHeader::size();
	RSVP_ObjectHeader tempObject;
	while (o.length < pDataOffset) { // read until offset (RSVP_ObjectHeaders)
		buffer >> tempObject;
		switch (tempObject.getClassNum()) {
			case RSVP_ObjectHeader::FILTER_SPEC:
				o.setFILTER_SPEC_Object(new FILTER_SPEC_Object(buffer));
				break;
			case RSVP_ObjectHeader::SCOPE:
				o.setSCOPE_Object(new SCOPE_Object(buffer, tempObject.getLength()));
				break;
			case RSVP_ObjectHeader::INTEGRITY:
				o.setINTEGRITY_Object(new INTEGRITY_Object(buffer));
				break;
			case RSVP_ObjectHeader::null:
				o.setPolicyRefreshPeriod(new PolicyRefreshPeriod(buffer));
				break;
			default:
				o.correct = false;
				LOG(3)( Log::PC, "WARNING: unknown object class num", tempObject.getClassNum(), "in POLICY_DATA -> ignoring object" );
				buffer.skip( tempObject.getLength() - RSVP_ObjectHeader::size() );
				o.length += tempObject.getLength();
				break;
		}
	}
	o.dataOffset = pDataOffset + RSVP_ObjectHeader::size();
	PolicyElement tempPolicyElement;
	while (o.length < policyDataObjectLength) { // read until end (PolicyElements)
		buffer >> tempPolicyElement;
		switch(tempPolicyElement.getPType()) {
			case PolicyElement::UPSTREAM_CHARGING_POLICY:
				o.addUCPE(new UCPE(buffer));
				break;
			default:
				o.correct = false;
				LOG(3)( Log::PC, "WARNING: unknown policy element", tempPolicyElement.getPType(), "in POLICY_DATA -> ignoring object" );
				buffer.skip( tempPolicyElement.getLength() - PolicyElement::size() );
				o.length += tempPolicyElement.getLength();
				break;
		}
	}
	return buffer;
}
		
ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const POLICY_DATA_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::POLICY_DATA, 1 );
	buffer << o.length<<o.dataOffset << o.defaultGateway;
	if (o.filterSpec) buffer << *o.filterSpec;
	if (o.scope) buffer << *o.scope;
	if (o.integrity) buffer << *o.integrity;
	if (o.policyRefreshPeriod) buffer << *o.policyRefreshPeriod;
	UCPEList::ConstIterator iter = o.ucpeList.begin();
	for (;iter != o.ucpeList.end(); ++iter) {
		buffer << **iter;
	}
	return buffer;
}

ostream& operator<< ( ostream& os, const POLICY_DATA_Object& o ) {
	os << o.length << " " << o.dataOffset << " " << o.defaultGateway;
	if (o.filterSpec) os << " " << *o.filterSpec;
	if (o.scope) os << " " << *o.scope;
	if (o.integrity) os << " " << *o.integrity;
	if (o.policyRefreshPeriod) os << " " << *o.policyRefreshPeriod;
	UCPEList::ConstIterator iter = o.ucpeList.begin();
	for (;iter != o.ucpeList.end(); ++iter) {
		os << " " << **iter;
	}
	return os;
}

INetworkBuffer& operator>> ( INetworkBuffer& buffer, INTEGRITY_Object& o ) {
	buffer >> o.content;
	return buffer;
}

ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const INTEGRITY_Object& o ) {
	buffer << RSVP_ObjectHeader( o.size(), RSVP_ObjectHeader::INTEGRITY, 1 );
	buffer << o.content;
	return buffer;
}

ostream& operator<< ( ostream& os , const INTEGRITY_Object& o ) {
	os << "INTEGRITY: " << o.content;
	return os;
}
