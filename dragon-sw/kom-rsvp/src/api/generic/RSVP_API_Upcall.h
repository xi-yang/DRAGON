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
#ifndef _RSVP_API_Upcall_h_
#define _RSVP_API_Upcall_h_ 1

#include "RSVP_API.h"
#include "RSVP_IntServObjects.h"
#include "RSVP_Lists.h"
#include "RSVP_PolicyObjects.h"

struct UpcallParameter {
	enum InfoType { PATH_EVENT, RESV_EVENT, PATH_TEAR, RESV_TEAR, PATH_ERROR, RESV_ERROR, RESV_CONFIRM };
	RSVP_API::SessionId session;
	InfoType infoType;
	friend inline ostream& operator<< ( ostream&, const UpcallParameter& );	
public:
	UpcallParameter( RSVP_API::SessionId session, InfoType infoType )
		: session(session), infoType(infoType) {}
	virtual ~UpcallParameter() {}
};

struct UpcallParameterPATH_EVENT : public UpcallParameter {
	SENDER_TEMPLATE_Object     senderTemplate;
	const SENDER_TSPEC_Object  sendTSpec;
	const ADSPEC_Object*       adSpec;
	const POLICY_DATA_Object*  policyData;
	friend inline ostream& operator<< ( ostream&, const UpcallParameterPATH_EVENT& );	
public:
	UpcallParameterPATH_EVENT( RSVP_API::SessionId session,
		const SENDER_TEMPLATE_Object& senderTemplate,
		const SENDER_TSPEC_Object& sendTSpec, const ADSPEC_Object* adSpec,
		const POLICY_DATA_Object* policyData )
	: UpcallParameter( session, PATH_EVENT ), senderTemplate(senderTemplate),
		sendTSpec(sendTSpec), adSpec(adSpec), policyData(policyData) {

		if (adSpec) adSpec->borrow();
		if (policyData) policyData->borrow();
	}

	~UpcallParameterPATH_EVENT() {
		if (adSpec) adSpec->destroy();
		if (policyData) policyData->destroy();
	}
};

struct UpcallParameterRESV_EVENT : public UpcallParameter {
	STYLE_Object              style;
	FlowDescriptor            flowDescriptor;
	const POLICY_DATA_Object* policyData;
	friend inline ostream& operator<< ( ostream&, const UpcallParameterRESV_EVENT& );	
public:
	UpcallParameterRESV_EVENT( RSVP_API::SessionId session,
		const STYLE_Object& style, const FlowDescriptor& flowDescriptor,
		const POLICY_DATA_Object* policyData )
	: UpcallParameter( session, RESV_EVENT ), style(style),
		flowDescriptor(flowDescriptor), policyData(policyData) {
		if (policyData) policyData->borrow();
	}

	~UpcallParameterRESV_EVENT() {
		if (policyData) policyData->destroy();
	}
};

struct UpcallParameterPATH_TEAR : public UpcallParameter {
	SENDER_TEMPLATE_Object  senderTemplate;
	RSVP_HOP_Object         hop;
	const PolicyObjectList& policyList;
	friend inline ostream& operator<< ( ostream&, const UpcallParameterPATH_TEAR& );	
public:
	UpcallParameterPATH_TEAR( RSVP_API::SessionId session,
		const SENDER_TEMPLATE_Object& senderTemplate, 
		const RSVP_HOP_Object& hop,
		const PolicyObjectList& policyList )
	: UpcallParameter( session, PATH_TEAR ), senderTemplate(senderTemplate),
		hop(hop), policyList(policyList) {}
};

struct UpcallParameterRESV_TEAR : public UpcallParameter {
	FlowDescriptor          flowDescriptor;
	RSVP_HOP_Object         hop;
	const PolicyObjectList& policyList;
	friend inline ostream& operator<< ( ostream&, const UpcallParameterRESV_TEAR& );	
public:
	UpcallParameterRESV_TEAR( RSVP_API::SessionId session,
		const FlowDescriptor& flowDescriptor, 
		const RSVP_HOP_Object& hop,
		const PolicyObjectList& policyList )
	: UpcallParameter( session, RESV_TEAR ), flowDescriptor(flowDescriptor),
		hop(hop), policyList(policyList) {}
};

struct UpcallParameterPATH_ERROR : public UpcallParameter {
	SENDER_TEMPLATE_Object  senderTemplate;
	ERROR_SPEC_Object       error;
	const PolicyObjectList& policyList;
	friend inline ostream& operator<< ( ostream&, const UpcallParameterPATH_ERROR& );	
public:
	UpcallParameterPATH_ERROR( RSVP_API::SessionId session,
		const SENDER_TEMPLATE_Object& senderTemplate,
		const ERROR_SPEC_Object& error,
		const PolicyObjectList& policyList )
	: UpcallParameter( session, PATH_ERROR ), senderTemplate(senderTemplate),
		error(error), policyList(policyList) {}
};

struct UpcallParameterRESV_ERROR : public UpcallParameter {
	FlowDescriptor          flowDescriptor;
	ERROR_SPEC_Object       error;
	RSVP_HOP_Object         hop;
	const PolicyObjectList& policyList;
	friend inline ostream& operator<< ( ostream&, const UpcallParameterRESV_ERROR& );	
public:
	UpcallParameterRESV_ERROR( RSVP_API::SessionId session,
		const FlowDescriptor& flowDescriptor, const ERROR_SPEC_Object& error,
		const RSVP_HOP_Object& hop, 
		const PolicyObjectList& policyList )
	: UpcallParameter( session, RESV_ERROR ), flowDescriptor(flowDescriptor),
		error(error), hop(hop), policyList(policyList) {}
};

struct UpcallParameterRESV_CONFIRM : public UpcallParameter {
	STYLE_Object            style;
	ERROR_SPEC_Object				error;
	FlowDescriptor          flowDescriptor;
	const PolicyObjectList& policyList;
	friend inline ostream& operator<< ( ostream&, const UpcallParameterRESV_CONFIRM& );	
public:
	UpcallParameterRESV_CONFIRM( RSVP_API::SessionId session,
		const STYLE_Object& style, const ERROR_SPEC_Object& error,
		const FlowDescriptor& flowDescriptor, const PolicyObjectList& policyList )
	: UpcallParameter( session, RESV_CONFIRM ), style(style), error(error),
		flowDescriptor(flowDescriptor), policyList(policyList) {}
};		

union GenericUpcallParameter {
	UpcallParameter* generalInfo;
	UpcallParameterPATH_EVENT* pathEvent;
	UpcallParameterRESV_EVENT* resvEvent;
	UpcallParameterPATH_TEAR*  pathTear;
	UpcallParameterRESV_TEAR*  resvTear;
	UpcallParameterPATH_ERROR* pathError;
	UpcallParameterRESV_ERROR* resvError;
	UpcallParameterRESV_CONFIRM* resvConfirm;
};

inline ostream& operator<< ( ostream& os, const UpcallParameter& u ) {
	os << *(SESSION_Object*)(*u.session) << " ";
	switch (u.infoType) {
		case UpcallParameter::PATH_EVENT: os << "PATH "; break;
		case UpcallParameter::RESV_EVENT: os << "RESV "; break;
		case UpcallParameter::PATH_TEAR: os << "PTEAR "; break;
		case UpcallParameter::RESV_TEAR: os << "RTEAR "; break;
		case UpcallParameter::PATH_ERROR: os << "PERR "; break;
		case UpcallParameter::RESV_ERROR: os << "RERR "; break;
		case UpcallParameter::RESV_CONFIRM: os << "RCONF "; break;
		default: os << "UNKNOWN "; break;
	}
	return os;
}

inline ostream& operator<< ( ostream& os, const UpcallParameterPATH_EVENT& u ) {
	os << (UpcallParameter&)u;
	os << u.senderTemplate << " " << u.sendTSpec;
	if (u.adSpec) os << " " << *u.adSpec;
	if (u.policyData) os << endl << *u.policyData;
	return os;
}

inline ostream& operator<< ( ostream& os, const UpcallParameterRESV_EVENT& u ) {
	os << (UpcallParameter)u;
	os << u.style;
	if (u.policyData) os << endl << *u.policyData;
	return os;
}

inline ostream& operator<< ( ostream& os, const UpcallParameterPATH_TEAR& u ) {
	os << (UpcallParameter)u;
	os << u.senderTemplate << " from phop " << u.hop;
	PolicyObjectList::ConstIterator iter = u.policyList.begin();
	for (; iter != u.policyList.end(); ++iter ) {
		os << endl << **iter;
	}
	return os;
}

inline ostream& operator<< ( ostream& os, const UpcallParameterRESV_TEAR& u ) {
	os << (UpcallParameter)u;
	os << u.flowDescriptor << " " << u.hop;
	PolicyObjectList::ConstIterator iter = u.policyList.begin();
	for (; iter != u.policyList.end(); ++iter ) {
		os << endl << **iter;
	}
	return os;
}

inline ostream& operator<< ( ostream& os, const UpcallParameterPATH_ERROR& u ) {
	os << (UpcallParameter)u;
	os << u.senderTemplate << " " << u.error;
	PolicyObjectList::ConstIterator iter = u.policyList.begin();
	for (; iter != u.policyList.end(); ++iter ) {
		os << endl << **iter;
	}
	return os;
}

inline ostream& operator<< ( ostream& os, const UpcallParameterRESV_ERROR& u ) {
	os << (UpcallParameter)u;
	os << u.flowDescriptor << " " << u.error << " " << u.hop;
	PolicyObjectList::ConstIterator iter = u.policyList.begin();
	for (; iter != u.policyList.end(); ++iter ) {
		os << endl << **iter;
	}
	return os;
}

inline ostream& operator<< ( ostream& os, const UpcallParameterRESV_CONFIRM& u ) {
	os << (UpcallParameter)u;
	os << u.style << " from " << u.error.getAddress() << " ";
	PolicyObjectList::ConstIterator iter = u.policyList.begin();
	for (; iter != u.policyList.end(); ++iter ) {
		os << endl << **iter;
	}
	return os;
}

#endif /* _RSVP_API_Upcall_h_ */
