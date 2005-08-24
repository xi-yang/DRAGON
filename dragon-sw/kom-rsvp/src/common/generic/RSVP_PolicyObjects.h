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
#ifndef _RSVP_PolicyObjects_h_
#define _RSVP_PolicyObjects_h_ 1

#include "RSVP_RefObject.h"
#include "RSVP_ObjectHeader.h"
#include "RSVP_PolicyElement.h"
#include "RSVP_List.h"
#include "RSVP_ChargingElements.h"

class FILTER_SPEC_Object;
class SCOPE_Object;
class INTEGRITY_Object;
class PolicyRefreshPeriod;

typedef SimpleList<const UCPE*> UCPEList;

class INTEGRITY_Object : public RefObject<INTEGRITY_Object> {
	uint32 content;
  friend ostream& operator<< ( ostream&, const INTEGRITY_Object& ); 
  friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const INTEGRITY_Object& );
  friend INetworkBuffer& operator>> ( INetworkBuffer&, INTEGRITY_Object& );
	REF_OBJECT_METHODS(INTEGRITY_Object)
public:
	INTEGRITY_Object( uint32 content ) : content(content) {}
	INTEGRITY_Object( INetworkBuffer& buffer ) { buffer >> *this; }
 	uint16 size() const { return 4; }
	uint32 getContent() const { return content; }
	uint16 total_size() const { return size() + RSVP_ObjectHeader::size(); }
};	
extern inline INTEGRITY_Object::~INTEGRITY_Object() {}

class POLICY_DATA_Object : public RefObject<POLICY_DATA_Object> {
	uint16 length; // corresponds to length field of common object header
	uint16 dataOffset; // begin of policy elements
	//uint16 reserved; // allways zero
	NetAddress defaultGateway; 
	
	// Native RSVP Options
	const FILTER_SPEC_Object *filterSpec;
	const SCOPE_Object *scope;
	const INTEGRITY_Object *integrity; // See above

	// Other Options
	const PolicyRefreshPeriod *policyRefreshPeriod;

	// Policy Elements
	UCPEList ucpeList;

	// check correctness of this object
	bool correct;
		
	friend ostream& operator<< ( ostream&, const POLICY_DATA_Object& ); 
  friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const POLICY_DATA_Object& );
  friend INetworkBuffer& operator>> ( INetworkBuffer&, POLICY_DATA_Object& );	
	REF_OBJECT_METHODS(POLICY_DATA_Object)
	void destructor();
public:

	POLICY_DATA_Object() 
		: length(NetAddress::size()+4), dataOffset(8), defaultGateway(0),
			filterSpec(NULL), scope(NULL),
			integrity(NULL), policyRefreshPeriod(NULL), correct(true) {}

	POLICY_DATA_Object( INetworkBuffer& buffer, uint16 length )
		: length(length-RSVP_ObjectHeader::size()), dataOffset(8), 
			filterSpec(NULL), scope(NULL),
			integrity(NULL), policyRefreshPeriod(NULL),
			correct(true) { buffer >> *this; }

	uint16 size() const { return length; }
	uint16 total_size() const { return size() + RSVP_ObjectHeader::size(); }
	static uint16 emptySize() { return NetAddress::size()+4; }
	void setFILTER_SPEC_Object( const FILTER_SPEC_Object *object );
	void setSCOPE_Object( const SCOPE_Object *object );
	void setINTEGRITY_Object( const INTEGRITY_Object *object );
	void setPolicyRefreshPeriod( const PolicyRefreshPeriod *object );
	void setDefaultGateway(const String &gw) {defaultGateway=NetAddress(gw);}
	void setDefaultGateway(const NetAddress &gw) {defaultGateway=gw;}
	 const NetAddress& getDefaultGateway() const {return defaultGateway;}
	 void addUCPE( const UCPE* object );
	const UCPEList& getUCPEList() const { return ucpeList; }
	bool checkCorrectness() const { return correct; }
};
extern inline POLICY_DATA_Object::~POLICY_DATA_Object() { destructor(); }

#endif /* _RSVP_PolicyObjects_h_ */
